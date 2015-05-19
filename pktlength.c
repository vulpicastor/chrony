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

  Routines to compute the expected length of a command or reply packet.
  These operate on the RAW NETWORK packets, from the point of view of
  integer endianness within the structures.

  */
#include "config.h"

#include "sysincl.h"

#include "util.h"
#include "pktlength.h"

/* ================================================== */

static int
command_unpadded_length(CMD_Request *r)
{
  int type;
  type = ntohs(r->command);
  if (type < 0 || type >= N_REQUEST_TYPES) {
    return 0;
  } else {
    switch (type) {
      
      case REQ_NULL:
        return offsetof(CMD_Request, data);
      case REQ_ONLINE:
        return offsetof(CMD_Request, data.online.EOR);
      case REQ_OFFLINE:
        return offsetof(CMD_Request, data.offline.EOR);
      case REQ_BURST:
        return offsetof(CMD_Request, data.burst.EOR);
      case REQ_MODIFY_MINPOLL:
        return offsetof(CMD_Request, data.modify_minpoll.EOR);
      case REQ_MODIFY_MAXPOLL:
        return offsetof(CMD_Request, data.modify_maxpoll.EOR);
      case REQ_DUMP:
        return offsetof(CMD_Request, data.dump.EOR);
      case REQ_MODIFY_MAXDELAY:
        return offsetof(CMD_Request, data.modify_maxdelay.EOR);
      case REQ_MODIFY_MAXDELAYRATIO:
        return offsetof(CMD_Request, data.modify_maxdelayratio.EOR);
      case REQ_MODIFY_MAXDELAYDEVRATIO:
        return offsetof(CMD_Request, data.modify_maxdelaydevratio.EOR);
      case REQ_MODIFY_MAXUPDATESKEW:
        return offsetof(CMD_Request, data.modify_maxupdateskew.EOR);
      case REQ_MODIFY_MAKESTEP:
        return offsetof(CMD_Request, data.modify_makestep.EOR);
      case REQ_LOGON :
        return offsetof(CMD_Request, data.logon.EOR);
      case REQ_SETTIME :
        return offsetof(CMD_Request, data.settime.EOR);
      case REQ_LOCAL :
        return offsetof(CMD_Request, data.local.EOR);
      case REQ_MANUAL :
        return offsetof(CMD_Request, data.manual.EOR);
      case REQ_N_SOURCES :
        return offsetof(CMD_Request, data.n_sources.EOR);
      case REQ_SOURCE_DATA :
        return offsetof(CMD_Request, data.source_data.EOR);
      case REQ_REKEY :
        return offsetof(CMD_Request, data.rekey.EOR);
      case REQ_ALLOW :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_ALLOWALL :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_DENY :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_DENYALL :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_CMDALLOW :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_CMDALLOWALL :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_CMDDENY :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_CMDDENYALL :
        return offsetof(CMD_Request, data.allow_deny.EOR);
      case REQ_ACCHECK :
        return offsetof(CMD_Request, data.ac_check.EOR);
      case REQ_CMDACCHECK :
        return offsetof(CMD_Request, data.ac_check.EOR);
      case REQ_ADD_SERVER :
        return offsetof(CMD_Request, data.ntp_source.EOR);
      case REQ_ADD_PEER :
        return offsetof(CMD_Request, data.ntp_source.EOR);
      case REQ_DEL_SOURCE :
        return offsetof(CMD_Request, data.del_source.EOR);
      case REQ_WRITERTC :
        return offsetof(CMD_Request, data.writertc.EOR);
      case REQ_DFREQ :
        return offsetof(CMD_Request, data.dfreq.EOR);
      case REQ_DOFFSET :
        return offsetof(CMD_Request, data.doffset.EOR);
      case REQ_TRACKING :
        return offsetof(CMD_Request, data.tracking.EOR);
      case REQ_SOURCESTATS :
        return offsetof(CMD_Request, data.sourcestats.EOR);
      case REQ_RTCREPORT :
        return offsetof(CMD_Request, data.rtcreport.EOR);
      case REQ_TRIMRTC :
        return offsetof(CMD_Request, data.trimrtc.EOR);
      case REQ_CYCLELOGS :
        return offsetof(CMD_Request, data.cyclelogs.EOR);
      case REQ_SUBNETS_ACCESSED :
      case REQ_CLIENT_ACCESSES:
        /* No longer supported */
        return 0;
      case REQ_CLIENT_ACCESSES_BY_INDEX:
        return offsetof(CMD_Request, data.client_accesses_by_index.EOR);
      case REQ_MANUAL_LIST:
        return offsetof(CMD_Request, data.manual_list.EOR);
      case REQ_MANUAL_DELETE:
        return offsetof(CMD_Request, data.manual_delete.EOR);
      case REQ_MAKESTEP:
        return offsetof(CMD_Request, data.make_step.EOR);
      case REQ_ACTIVITY:
        return offsetof(CMD_Request, data.activity.EOR);
      case REQ_RESELECT:
        return offsetof(CMD_Request, data.reselect.EOR);
      case REQ_RESELECTDISTANCE:
        return offsetof(CMD_Request, data.reselect_distance.EOR);
      case REQ_MODIFY_MINSTRATUM:
        return offsetof(CMD_Request, data.modify_minstratum.EOR);
      case REQ_MODIFY_POLLTARGET:
        return offsetof(CMD_Request, data.modify_polltarget.EOR);
      default:
        /* If we fall through the switch, it most likely means we've forgotten to implement a new case */
        assert(0);
    }
  }

  /* Catch-all case */
  return 0;

}


/* ================================================== */

int
PKL_CommandLength(CMD_Request *r)
{
  int command_length;

  command_length = command_unpadded_length(r);
  if (!command_length)
    return 0;

  return command_length + PKL_CommandPaddingLength(r);
}

/* ================================================== */

#define PADDING_LENGTH_(request_length, reply_length) \
  ((request_length) < (reply_length) ? (reply_length) - (request_length) : 0)

#define PADDING_LENGTH(request_data, reply_data) \
  PADDING_LENGTH_(offsetof(CMD_Request, request_data), offsetof(CMD_Reply, reply_data))

int
PKL_CommandPaddingLength(CMD_Request *r)
{
  int type;

  if (r->version < PROTO_VERSION_PADDING)
    return 0;

  type = ntohs(r->command);

  if (type < 0 || type >= N_REQUEST_TYPES)
    return 0;

  switch (type) {
    case REQ_NULL:
      return PADDING_LENGTH(data, data.null.EOR);
    case REQ_ONLINE:
      return PADDING_LENGTH(data.online.EOR, data.null.EOR);
    case REQ_OFFLINE:
      return PADDING_LENGTH(data.offline.EOR, data.null.EOR);
    case REQ_BURST:
      return PADDING_LENGTH(data.burst.EOR, data.null.EOR);
    case REQ_MODIFY_MINPOLL:
      return PADDING_LENGTH(data.modify_minpoll.EOR, data.null.EOR);
    case REQ_MODIFY_MAXPOLL:
      return PADDING_LENGTH(data.modify_maxpoll.EOR, data.null.EOR);
    case REQ_DUMP:
      return PADDING_LENGTH(data.dump.EOR, data.null.EOR);
    case REQ_MODIFY_MAXDELAY:
      return PADDING_LENGTH(data.modify_maxdelay.EOR, data.null.EOR);
    case REQ_MODIFY_MAXDELAYRATIO:
      return PADDING_LENGTH(data.modify_maxdelayratio.EOR, data.null.EOR);
    case REQ_MODIFY_MAXDELAYDEVRATIO:
      return PADDING_LENGTH(data.modify_maxdelaydevratio.EOR, data.null.EOR);
    case REQ_MODIFY_MAXUPDATESKEW:
      return PADDING_LENGTH(data.modify_maxupdateskew.EOR, data.null.EOR);
    case REQ_MODIFY_MAKESTEP:
      return PADDING_LENGTH(data.modify_makestep.EOR, data.null.EOR);
    case REQ_LOGON:
      return PADDING_LENGTH(data.logon.EOR, data.null.EOR);
    case REQ_SETTIME:
      return PADDING_LENGTH(data.settime.EOR, data.manual_timestamp.EOR);
    case REQ_LOCAL:
      return PADDING_LENGTH(data.local.EOR, data.null.EOR);
    case REQ_MANUAL:
      return PADDING_LENGTH(data.manual.EOR, data.null.EOR);
    case REQ_N_SOURCES:
      return PADDING_LENGTH(data.n_sources.EOR, data.n_sources.EOR);
    case REQ_SOURCE_DATA:
      return PADDING_LENGTH(data.source_data.EOR, data.source_data.EOR);
    case REQ_REKEY:
      return PADDING_LENGTH(data.rekey.EOR, data.null.EOR);
    case REQ_ALLOW:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_ALLOWALL:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_DENY:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_DENYALL:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_CMDALLOW:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_CMDALLOWALL:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_CMDDENY:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_CMDDENYALL:
      return PADDING_LENGTH(data.allow_deny.EOR, data.null.EOR);
    case REQ_ACCHECK:
      return PADDING_LENGTH(data.ac_check.EOR, data.null.EOR);
    case REQ_CMDACCHECK:
      return PADDING_LENGTH(data.ac_check.EOR, data.null.EOR);
    case REQ_ADD_SERVER:
      return PADDING_LENGTH(data.ntp_source.EOR, data.null.EOR);
    case REQ_ADD_PEER:
      return PADDING_LENGTH(data.ntp_source.EOR, data.null.EOR);
    case REQ_DEL_SOURCE:
      return PADDING_LENGTH(data.del_source.EOR, data.null.EOR);
    case REQ_WRITERTC:
      return PADDING_LENGTH(data.writertc.EOR, data.null.EOR);
    case REQ_DFREQ:
      return PADDING_LENGTH(data.dfreq.EOR, data.null.EOR);
    case REQ_DOFFSET:
      return PADDING_LENGTH(data.doffset.EOR, data.null.EOR);
    case REQ_TRACKING:
      return PADDING_LENGTH(data.tracking.EOR, data.tracking.EOR);
    case REQ_SOURCESTATS:
      return PADDING_LENGTH(data.sourcestats.EOR, data.sourcestats.EOR);
    case REQ_RTCREPORT:
      return PADDING_LENGTH(data.rtcreport.EOR, data.rtc.EOR);
    case REQ_TRIMRTC:
      return PADDING_LENGTH(data.trimrtc.EOR, data.null.EOR);
    case REQ_CYCLELOGS:
      return PADDING_LENGTH(data.cyclelogs.EOR, data.null.EOR);
    case REQ_SUBNETS_ACCESSED:
    case REQ_CLIENT_ACCESSES:
      /* No longer supported */
      return 0;
    case REQ_CLIENT_ACCESSES_BY_INDEX:
      return PADDING_LENGTH(data.client_accesses_by_index.EOR, data.client_accesses_by_index.EOR);
    case REQ_MANUAL_LIST:
      return PADDING_LENGTH(data.manual_list.EOR, data.manual_list.EOR);
    case REQ_MANUAL_DELETE:
      return PADDING_LENGTH(data.manual_delete.EOR, data.null.EOR);
    case REQ_MAKESTEP:
      return PADDING_LENGTH(data.make_step.EOR, data.null.EOR);
    case REQ_ACTIVITY:
      return PADDING_LENGTH(data.activity.EOR, data.activity.EOR);
    case REQ_RESELECT:
      return PADDING_LENGTH(data.reselect.EOR, data.null.EOR);
    case REQ_RESELECTDISTANCE:
      return PADDING_LENGTH(data.reselect_distance.EOR, data.null.EOR);
    case REQ_MODIFY_MINSTRATUM:
      return PADDING_LENGTH(data.modify_minstratum.EOR, data.null.EOR);
    case REQ_MODIFY_POLLTARGET:
      return PADDING_LENGTH(data.modify_polltarget.EOR, data.null.EOR);
    default:
      /* If we fall through the switch, it most likely means we've forgotten to implement a new case */
      assert(0);
      return 0;
  }
}

/* ================================================== */

int
PKL_ReplyLength(CMD_Reply *r)
{
  int type;
  type = ntohs(r->reply);
  /* Note that reply type codes start from 1, not 0 */
  if (type < 1 || type >= N_REPLY_TYPES) {
    return 0;
  } else {
    switch (type) {
      case RPY_NULL:
        return offsetof(CMD_Reply, data.null.EOR);
      case RPY_N_SOURCES:
        return offsetof(CMD_Reply, data.n_sources.EOR);
      case RPY_SOURCE_DATA:
        return offsetof(CMD_Reply, data.source_data.EOR);
      case RPY_MANUAL_TIMESTAMP:
        return offsetof(CMD_Reply, data.manual_timestamp.EOR);
      case RPY_TRACKING:
        return offsetof(CMD_Reply, data.tracking.EOR);
      case RPY_SOURCESTATS:
        return offsetof(CMD_Reply, data.sourcestats.EOR);
      case RPY_RTC:
        return offsetof(CMD_Reply, data.rtc.EOR);
      case RPY_SUBNETS_ACCESSED :
      case RPY_CLIENT_ACCESSES:
        /* No longer supported */
        return 0;
      case RPY_CLIENT_ACCESSES_BY_INDEX:
        {
          unsigned long nc = ntohl(r->data.client_accesses_by_index.n_clients);
          if (r->status == htons(STT_SUCCESS)) {
            if (nc > MAX_CLIENT_ACCESSES)
              return 0;
            return (offsetof(CMD_Reply, data.client_accesses_by_index.clients) +
                    nc * sizeof(RPY_ClientAccesses_Client));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_MANUAL_LIST:
        {
          unsigned long ns = ntohl(r->data.manual_list.n_samples);
          if (ns > MAX_MANUAL_LIST_SAMPLES)
            return 0;
          if (r->status == htons(STT_SUCCESS)) {
            return (offsetof(CMD_Reply, data.manual_list.samples) +
                    ns * sizeof(RPY_ManualListSample));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_ACTIVITY:
        return offsetof(CMD_Reply, data.activity.EOR);
        
      default:
        assert(0);
    }
  }

  return 0;
}

/* ================================================== */

