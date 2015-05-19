/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Miroslav Lichvar  2015
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

  Routines implementing time smoothing.

  */

#include "config.h"

#include "sysincl.h"

#include "conf.h"
#include "local.h"
#include "logging.h"
#include "reference.h"
#include "smooth.h"
#include "util.h"

/*
  Time smoothing determines an offset that needs to be applied to the cooked
  time to make it smooth for external observers.  Observed offset and frequency
  change slowly and there are no discontinuities.  This can be used on an NTP
  server to make it easier for the clients to track the time and keep their
  clocks close together even when large offset or frequency corrections are
  applied to the server's clock (e.g. after being offline for longer time).

  Accumulated offset and frequency are smoothed out in three stages.  In the
  first stage, the frequency is changed at a constant rate (wander) up to a
  maximum, in the second stage the frequency stays at the maximum for as long
  as needed and in the third stage the frequency is brought back to zero.

              |
    max_freq  +-------/--------\-------------
              |      /|        |\
        freq  |     / |        | \
              |    /  |        |  \
              |   /   |        |   \
           0  +--/----+--------+----\--------
              | /     |        |    |    time
              |/      |        |    |

        stage     1       2      3

  Integral of this function is the smoothed out offset.  It's a continuous
  piecewise polynomial with two quadratic parts and one linear.
*/

struct stage {
  double wander;
  double length;
};

#define NUM_STAGES 3

static struct stage stages[NUM_STAGES];

/* Enabled/disabled smoothing */
static int enabled;

/* Maximum skew/max_wander ratio to start updating offset and frequency */
#define UNLOCK_SKEW_WANDER_RATIO 10000

static int locked;

/* Maximum wander and frequency offset */
static double max_wander;
static double max_freq;

/* Frequency offset, time offset and the time of the last smoothing update */
static double smooth_freq;
static double smooth_offset;
static struct timeval last_update;


static void
get_offset_freq(struct timeval *now, double *offset, double *freq)
{
  double elapsed, length;
  int i;

  UTI_DiffTimevalsToDouble(&elapsed, now, &last_update);

  *offset = smooth_offset;
  *freq = smooth_freq;

  for (i = 0; i < NUM_STAGES; i++) {
    if (elapsed <= 0.0)
      break;

    length = stages[i].length;
    if (length >= elapsed)
      length = elapsed;

    *offset -= length * (2.0 * *freq + stages[i].wander * length) / 2.0;
    *freq += stages[i].wander * length;
    elapsed -= length;
  }

  if (elapsed > 0.0)
    *offset -= elapsed * *freq;
}

static void
update_stages(void)
{
  double s1, s2, s, l1, l2, l3, lc, f, f2;
  int i, dir;

  /* Prepare the three stages so that the integral of the frequency offset
     is equal to the offset that should be smoothed out */

  s1 = smooth_offset / max_wander;
  s2 = smooth_freq * smooth_freq / (2.0 * max_wander * max_wander);
  
  l1 = l2 = l3 = 0.0;

  /* Calculate the lengths of the 1st and 3rd stage assuming there is no
     frequency limit.  If length of the 1st stage comes out negative, switch
     its direction. */
  for (dir = -1; dir <= 1; dir += 2) {
    s = dir * s1 + s2;
    if (s >= 0.0) {
      l3 = sqrt(s);
      l1 = l3 - dir * smooth_freq / max_wander;
      if (l1 >= 0.0)
        break;
    }
  }

  assert(dir <= 1 && l1 >= 0.0 && l3 >= 0.0);

  /* If the limit was reached, shorten 1st+3rd stages and set a 2nd stage */
  f = dir * smooth_freq + l1 * max_wander - max_freq;
  if (f > 0.0) {
    lc = f / max_wander;

    /* No 1st stage if the frequency is already above the maximum */
    if (lc > l1) {
      lc = l1;
      f2 = dir * smooth_freq;
    } else {
      f2 = max_freq;
    }

    l2 = lc * (2.0 + f / f2);
    l1 -= lc;
    l3 -= lc;
  }

  stages[0].wander = dir * max_wander;
  stages[0].length = l1;
  stages[1].wander = 0.0;
  stages[1].length = l2;
  stages[2].wander = -dir * max_wander;
  stages[2].length = l3;

  for (i = 0; i < NUM_STAGES; i++) {
    DEBUG_LOG(LOGF_Smooth, "Smooth stage %d wander %e length %f",
              i + 1, stages[i].wander, stages[i].length);
  }
}

static void
update_smoothing(struct timeval *now, double offset, double freq)
{
  /* Don't accept offset/frequency until the clock has stabilized */
  if (locked) {
    if (REF_GetSkew() / max_wander < UNLOCK_SKEW_WANDER_RATIO) {
      LOG(LOGS_INFO, LOGF_Smooth, "Time smoothing activated");
      locked = 0;
    }
    return;
  }

  get_offset_freq(now, &smooth_offset, &smooth_freq);
  smooth_offset += offset;
  smooth_freq = (smooth_freq - freq) / (1.0 - freq);
  last_update = *now;

  update_stages();

  DEBUG_LOG(LOGF_Smooth, "Smooth offset %e freq %e", smooth_offset, smooth_freq);
}

static void
handle_slew(struct timeval *raw, struct timeval *cooked, double dfreq,
            double doffset, LCL_ChangeType change_type, void *anything)
{
  double delta;

  if (change_type == LCL_ChangeAdjust)
    update_smoothing(cooked, doffset, dfreq);

  UTI_AdjustTimeval(&last_update, cooked, &last_update, &delta, dfreq, doffset);
}

void SMT_Initialise(void)
{
  CNF_GetSmooth(&max_freq, &max_wander);
  if (max_freq <= 0.0 || max_wander <= 0.0) {
      enabled = 0;
      return;
  }

  enabled = 1;
  locked = 1;

  /* Convert from ppm */
  max_freq *= 1e-6;
  max_wander *= 1e-6;

  LCL_AddParameterChangeHandler(handle_slew, NULL);
}

void SMT_Finalise(void)
{
}

int SMT_IsEnabled(void)
{
  return enabled;
}

double
SMT_GetOffset(struct timeval *now)
{
  double offset, freq;

  if (!enabled)
    return 0.0;
  
  get_offset_freq(now, &offset, &freq);

  return offset;
}

void
SMT_Reset(struct timeval *now)
{
  if (!enabled)
    return;

  locked = 1;
  smooth_offset = 0.0;
  smooth_freq = 0.0;
  last_update = *now;
}
