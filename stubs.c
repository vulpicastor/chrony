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

  Function replacements needed when optional features are disabled.

  */

#include "config.h"

#include "clientlog.h"
#include "cmdmon.h"
#include "keys.h"
#include "logging.h"
#include "manual.h"
#include "nameserv.h"
#include "nameserv_async.h"
#include "ntp_core.h"
#include "ntp_io.h"
#include "ntp_sources.h"
#include "refclock.h"

#ifndef FEAT_ASYNCDNS

#define MAX_ADDRESSES 16

/* This is a blocking implementation used when asynchronous resolving is not available */

void
DNS_Name2IPAddressAsync(const char *name, DNS_NameResolveHandler handler, void *anything)
{
  IPAddr addrs[MAX_ADDRESSES];
  DNS_Status status;
  int i;

  status = DNS_Name2IPAddress(name, addrs, MAX_ADDRESSES);

  for (i = 0; status == DNS_Success && i < MAX_ADDRESSES &&
              addrs[i].family != IPADDR_UNSPEC; i++)
    ;

  (handler)(status, i, addrs, anything);
}

#endif /* !FEAT_ASYNCDNS */

#ifndef FEAT_CMDMON

void
CAM_Initialise(int family)
{
}

void
CAM_Finalise(void)
{
}

int
CAM_AddAccessRestriction(IPAddr *ip_addr, int subnet_bits, int allow, int all)
{
  return 1;
}

void
MNL_Initialise(void)
{
}

void
MNL_Finalise(void)
{
}

#endif /* !FEAT_CMDMON */

#ifndef FEAT_NTP

void
NCR_AddBroadcastDestination(IPAddr *addr, unsigned short port, int interval)
{
}

void
NCR_Initialise(void)
{
}

void
NCR_Finalise(void)
{
}

int
NCR_AddAccessRestriction(IPAddr *ip_addr, int subnet_bits, int allow, int all)
{
  return 1;
}

int
NCR_CheckAccessRestriction(IPAddr *ip_addr)
{
  return 0;
}

void
NIO_Initialise(int family)
{
}

void
NIO_Finalise(void)
{
}

void
NSR_Initialise(void)
{
}

void
NSR_Finalise(void)
{
}

NSR_Status
NSR_AddSource(NTP_Remote_Address *remote_addr, NTP_Source_Type type, SourceParameters *params)
{
  return NSR_TooManySources;
}

void
NSR_AddSourceByName(char *name, int port, int pool, NTP_Source_Type type, SourceParameters *params)
{
}

NSR_Status
NSR_RemoveSource(NTP_Remote_Address *remote_addr)
{
  return NSR_NoSuchSource;
}

void
NSR_RemoveAllSources(void)
{
}

void
NSR_HandleBadSource(IPAddr *address)
{
}

void
NSR_SetSourceResolvingEndHandler(NSR_SourceResolvingEndHandler handler)
{
  if (handler)
    (handler)();
}

void
NSR_ResolveSources(void)
{
}

void NSR_StartSources(void)
{
}

void NSR_AutoStartSources(void)
{
}

int
NSR_InitiateSampleBurst(int n_good_samples, int n_total_samples,
                        IPAddr *mask, IPAddr *address)
{
  return 0;
}

int
NSR_TakeSourcesOnline(IPAddr *mask, IPAddr *address)
{
  return 0;
}

int
NSR_TakeSourcesOffline(IPAddr *mask, IPAddr *address)
{
  return 0;
}

int
NSR_ModifyMinpoll(IPAddr *address, int new_minpoll)
{
  return 0;
}

int
NSR_ModifyMaxpoll(IPAddr *address, int new_maxpoll)
{
  return 0;
}

int
NSR_ModifyMaxdelay(IPAddr *address, double new_max_delay)
{
  return 0;
}

int
NSR_ModifyMaxdelayratio(IPAddr *address, double new_max_delay_ratio)
{
  return 0;
}

int
NSR_ModifyMaxdelaydevratio(IPAddr *address, double new_max_delay_dev_ratio)
{
  return 0;
}

int
NSR_ModifyMinstratum(IPAddr *address, int new_min_stratum)
{
  return 0;
}

int
NSR_ModifyPolltarget(IPAddr *address, int new_poll_target)
{
  return 0;
}

void
NSR_ReportSource(RPT_SourceReport *report, struct timeval *now)
{
  memset(report, 0, sizeof (*report));
}
  
void
NSR_GetActivityReport(RPT_ActivityReport *report)
{
  memset(report, 0, sizeof (*report));
}

#ifndef FEAT_CMDMON

void
CLG_Initialise(void)
{
}

void
CLG_Finalise(void)
{
}

void
DNS_SetAddressFamily(int family)
{
}

DNS_Status
DNS_Name2IPAddress(const char *name, IPAddr *ip_addrs, int max_addrs)
{
  return DNS_Failure;
}

void
KEY_Initialise(void)
{
}

void
KEY_Finalise(void)
{
}

#endif /* !FEAT_CMDMON */
#endif /* !FEAT_NTP */

#ifndef FEAT_REFCLOCK
void
RCL_Initialise(void)
{
}

void
RCL_Finalise(void)
{
}

int
RCL_AddRefclock(RefclockParameters *params)
{
  return 0;
}

void
RCL_StartRefclocks(void)
{
}

void
RCL_ReportSource(RPT_SourceReport *report, struct timeval *now)
{
  memset(report, 0, sizeof (*report));
}

#endif /* !FEAT_REFCLOCK */
