/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2002
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

  Private include file for local.c and all system dependent
  driver modules.
  */


#ifndef GOT_LOCALP_H
#define GOT_LOCALP_H

/* System driver to read the current local frequency, in ppm relative
   to nominal.  A positive value indicates that the local clock runs
   fast when uncompensated. */
typedef double (*lcl_ReadFrequencyDriver)(void);

/* System driver to set the current local frequency, in ppm relative
   to nominal.  A positive value indicates that the local clock runs
   fast when uncompensated.  Return actual frequency (may be different
   from the requested frequency due to clamping or rounding). */
typedef double (*lcl_SetFrequencyDriver)(double freq_ppm);

/* System driver to accrue an offset. A positive argument means slew
   the clock forwards.  The suggested correction rate of time to correct the
   offset is given in 'corr_rate'. */
typedef void (*lcl_AccrueOffsetDriver)(double offset, double corr_rate);

/* System driver to apply a step offset. A positive argument means step
   the clock forwards. */
typedef void (*lcl_ApplyStepOffsetDriver)(double offset);

/* System driver to convert a raw time to an adjusted (cooked) time.
   The number of seconds returned in 'corr' have to be added to the
   raw time to get the corrected time */
typedef void (*lcl_OffsetCorrectionDriver)(struct timeval *raw, double *corr, double *err);

/* System driver to schedule leap second */
typedef void (*lcl_SetLeapDriver)(int leap);

extern void lcl_InvokeDispersionNotifyHandlers(double dispersion);

extern void
lcl_RegisterSystemDrivers(lcl_ReadFrequencyDriver read_freq,
                          lcl_SetFrequencyDriver set_freq,
                          lcl_AccrueOffsetDriver accrue_offset,
                          lcl_ApplyStepOffsetDriver apply_step_offset,
                          lcl_OffsetCorrectionDriver offset_convert,
                          lcl_SetLeapDriver set_leap);

#endif /* GOT_LOCALP_H */
