/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Miroslav Lichvar  2014
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 **********************************************************************

  =======================================================================

  Generic driver functions to complete system-specific drivers
  */

#include "config.h"

#include "sysincl.h"

#include "sys_generic.h"

#include "conf.h"
#include "local.h"
#include "localp.h"
#include "logging.h"
#include "sched.h"
#include "util.h"

/* ================================================== */

/* System clock drivers */
static lcl_ReadFrequencyDriver drv_read_freq;
static lcl_SetFrequencyDriver drv_set_freq;
static lcl_SetSyncStatusDriver drv_set_sync_status;

/* Current frequency as requested by the local module (in ppm) */
static double base_freq;

/* Maximum frequency that can be set by drv_set_freq (in ppm) */
static double max_freq;

/* Maximum expected delay in the actual frequency change (e.g. kernel ticks)
   in local time */
static double max_freq_change_delay;

/* Maximum allowed frequency offset relative to the base frequency */
static double max_corr_freq;

/* Amount of outstanding offset to process */
static double offset_register;

/* Minimum offset to correct */
#define MIN_OFFSET_CORRECTION 1.0e-9

/* Current frequency offset between base_freq and the real clock frequency
   as set by drv_set_freq (not in ppm) */
static double slew_freq;

/* Time (raw) of last update of slewing frequency and offset */
static struct timeval slew_start;

/* Limits for the slew timeout */
#define MIN_SLEW_TIMEOUT 1.0
#define MAX_SLEW_TIMEOUT 1.0e4

/* Scheduler timeout ID and flag if the timer is currently running */
static SCH_TimeoutID slew_timeout_id;
static int slew_timer_running;

/* Suggested offset correction rate (correction time * offset) */
static double correction_rate;

/* Maximum expected offset correction error caused by delayed change in the
   real frequency of the clock */
static double slew_error;

/* ================================================== */

static void handle_end_of_slew(void *anything);
static void update_slew(void);

/* ================================================== */
/* Adjust slew_start on clock step */

static void
handle_step(struct timeval *raw, struct timeval *cooked, double dfreq,
            double doffset, LCL_ChangeType change_type, void *anything)
{
  if (change_type == LCL_ChangeUnknownStep) {
    /* Reset offset and slewing */
    slew_start = *raw;
    offset_register = 0.0;
    update_slew();
  } else if (change_type == LCL_ChangeStep) {
    UTI_AddDoubleToTimeval(&slew_start, -doffset, &slew_start);
  }
}

/* ================================================== */

static double
clamp_freq(double freq)
{
  if (freq > max_freq)
    return max_freq;
  if (freq < -max_freq)
    return -max_freq;
  return freq;
}

/* ================================================== */
/* End currently running slew and start a new one */

static void
update_slew(void)
{
  struct timeval now, end_of_slew;
  double old_slew_freq, total_freq, corr_freq, duration;

  /* Remove currently running timeout */
  if (slew_timer_running)
    SCH_RemoveTimeout(slew_timeout_id);

  LCL_ReadRawTime(&now);

  /* Adjust the offset register by achieved slew */
  UTI_DiffTimevalsToDouble(&duration, &now, &slew_start);
  offset_register -= slew_freq * duration;

  /* Estimate how long should the next slew take */
  if (fabs(offset_register) < MIN_OFFSET_CORRECTION) {
    duration = MAX_SLEW_TIMEOUT;
  } else {
    duration = correction_rate / fabs(offset_register);
    if (duration < MIN_SLEW_TIMEOUT)
      duration = MIN_SLEW_TIMEOUT;
  }

  /* Get frequency offset needed to slew the offset in the duration
     and clamp it to the allowed maximum */
  corr_freq = offset_register / duration;
  if (corr_freq < -max_corr_freq)
    corr_freq = -max_corr_freq;
  else if (corr_freq > max_corr_freq)
    corr_freq = max_corr_freq;

  /* Get the new real frequency and clamp it */
  total_freq = clamp_freq(base_freq + corr_freq * (1.0e6 - base_freq));

  /* Set the new frequency (the actual frequency returned by the call may be
     slightly different from the requested frequency due to rounding) */
  total_freq = (*drv_set_freq)(total_freq);

  /* Compute the new slewing frequency, it's relative to the real frequency to
     make the calculation in offset_convert() cheaper */
  old_slew_freq = slew_freq;
  slew_freq = (total_freq - base_freq) / (1.0e6 - total_freq);

  /* Compute the dispersion introduced by changing frequency and add it
     to all statistics held at higher levels in the system */
  slew_error = fabs((old_slew_freq - slew_freq) * max_freq_change_delay);
  if (slew_error >= MIN_OFFSET_CORRECTION)
    lcl_InvokeDispersionNotifyHandlers(slew_error);

  /* Compute the duration of the slew and clamp it.  If the slewing frequency
     is zero or has wrong sign (e.g. due to rounding in the frequency driver or
     when base_freq is larger than max_freq), use maximum timeout and try again
     on the next update. */
  if (fabs(offset_register) < MIN_OFFSET_CORRECTION ||
      offset_register * slew_freq <= 0.0) {
    duration = MAX_SLEW_TIMEOUT;
  } else {
    duration = offset_register / slew_freq;
    if (duration < MIN_SLEW_TIMEOUT)
      duration = MIN_SLEW_TIMEOUT;
    else if (duration > MAX_SLEW_TIMEOUT)
      duration = MAX_SLEW_TIMEOUT;
  }

  /* Restart timer for the next update */
  UTI_AddDoubleToTimeval(&now, duration, &end_of_slew);
  slew_timeout_id = SCH_AddTimeout(&end_of_slew, handle_end_of_slew, NULL);

  slew_start = now;
  slew_timer_running = 1;

  DEBUG_LOG(LOGF_SysGeneric, "slew offset=%e corr_rate=%e base_freq=%f total_freq=%f slew_freq=%e duration=%f slew_error=%e",
      offset_register, correction_rate, base_freq, total_freq, slew_freq,
      duration, slew_error);
}

/* ================================================== */

static void
handle_end_of_slew(void *anything)
{
  slew_timer_running = 0;
  update_slew();
}

/* ================================================== */

static double
read_frequency(void)
{
  return base_freq;
}

/* ================================================== */

static double
set_frequency(double freq_ppm)
{
  base_freq = freq_ppm;
  update_slew();

  return base_freq;
}

/* ================================================== */

static void
accrue_offset(double offset, double corr_rate)
{
  offset_register += offset;
  correction_rate = corr_rate;

  update_slew();
}

/* ================================================== */
/* Determine the correction to generate the cooked time for given raw time */

static void
offset_convert(struct timeval *raw,
               double *corr, double *err)
{
  double duration;

  UTI_DiffTimevalsToDouble(&duration, raw, &slew_start);

  *corr = slew_freq * duration - offset_register;
  if (err)
    *err = fabs(duration) <= max_freq_change_delay ? slew_error : 0.0;
}

/* ================================================== */
/* Positive means currently fast of true time, i.e. jump backwards */

static int
apply_step_offset(double offset)
{
  struct timeval old_time, new_time;
  double err;

  LCL_ReadRawTime(&old_time);
  UTI_AddDoubleToTimeval(&old_time, -offset, &new_time);

  if (settimeofday(&new_time, NULL) < 0) {
    DEBUG_LOG(LOGF_SysGeneric, "settimeofday() failed");
    return 0;
  }

  LCL_ReadRawTime(&old_time);
  UTI_DiffTimevalsToDouble(&err, &old_time, &new_time);

  lcl_InvokeDispersionNotifyHandlers(fabs(err));

  return 1;
}

/* ================================================== */

static void
set_sync_status(int synchronised, double est_error, double max_error)
{
  double offset;

  offset = fabs(offset_register);
  if (est_error < offset)
    est_error = offset;
  max_error += offset;

  if (drv_set_sync_status)
    drv_set_sync_status(synchronised, est_error, max_error);
}

/* ================================================== */

void
SYS_Generic_CompleteFreqDriver(double max_set_freq_ppm, double max_set_freq_delay,
                               lcl_ReadFrequencyDriver sys_read_freq,
                               lcl_SetFrequencyDriver sys_set_freq,
                               lcl_ApplyStepOffsetDriver sys_apply_step_offset,
                               lcl_SetLeapDriver sys_set_leap,
                               lcl_SetSyncStatusDriver sys_set_sync_status)
{
  max_freq = max_set_freq_ppm;
  max_freq_change_delay = max_set_freq_delay * (1.0 + max_freq / 1.0e6);
  drv_read_freq = sys_read_freq;
  drv_set_freq = sys_set_freq;
  drv_set_sync_status = sys_set_sync_status;

  base_freq = (*drv_read_freq)();
  slew_freq = 0.0;
  offset_register = 0.0;

  max_corr_freq = CNF_GetMaxSlewRate() / 1.0e6;

  lcl_RegisterSystemDrivers(read_frequency, set_frequency,
                            accrue_offset, sys_apply_step_offset ?
                              sys_apply_step_offset : apply_step_offset,
                            offset_convert, sys_set_leap, set_sync_status);

  LCL_AddParameterChangeHandler(handle_step, NULL);
}

/* ================================================== */

void
SYS_Generic_Finalise(void)
{
  /* Must *NOT* leave a slew running - clock could drift way off
     if the daemon is not restarted */
  if (slew_timer_running) {
    SCH_RemoveTimeout(slew_timeout_id);
    slew_timer_running = 0;
  }

  (*drv_set_freq)(clamp_freq(base_freq));
}

/* ================================================== */
