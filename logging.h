/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2002
 * Copyright (C) Miroslav Lichvar  2013-2014
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

  Header file for diagnostic logging module

  */

#ifndef GOT_LOGGING_H
#define GOT_LOGGING_H

#include "sysincl.h"

/* Flag indicating whether debug messages are logged */
extern int log_debug_enabled;

/* Line logging macros.  If the compiler is GNU C, we take advantage of
   being able to get the function name also. */

#ifdef __GNUC__
#define FUNCTION_NAME __FUNCTION__
#define FORMAT_ATTRIBUTE_PRINTF(str, first) __attribute__ ((format (printf, str, first)))
#else
#define FUNCTION_NAME ""
#define FORMAT_ATTRIBUTE_PRINTF(str, first)
#endif

#define DEBUG_LOG(facility, ...) \
  do { \
    if (DEBUG && log_debug_enabled) \
      LOG_Message(LOGS_DEBUG, facility, __LINE__, __FILE__, FUNCTION_NAME, __VA_ARGS__); \
  } while (0)
#define LOG(severity, facility, ...) LOG_Message(severity, facility, __LINE__, __FILE__, FUNCTION_NAME, __VA_ARGS__)
#define LOG_FATAL(facility, ...) \
  do { \
    LOG_Message(LOGS_FATAL, facility, __LINE__, __FILE__, FUNCTION_NAME, __VA_ARGS__); \
    exit(1); \
  } while (0)

/* Definition of severity */
typedef enum {
  LOGS_INFO,
  LOGS_WARN,
  LOGS_ERR,
  LOGS_FATAL,
  LOGS_DEBUG
} LOG_Severity;

/* Definition of facility.  Each message is tagged with who generated
   it, so that the user can customise what level of reporting he gets
   for each area of the software */
typedef enum {
  LOGF_Reference,
  LOGF_NtpIO,
  LOGF_NtpCore,
  LOGF_NtpSources,
  LOGF_Scheduler,
  LOGF_SourceStats,
  LOGF_Sources,
  LOGF_Local,
  LOGF_Util,
  LOGF_Main,
  LOGF_Memory,
  LOGF_ClientLog,
  LOGF_Configure,
  LOGF_CmdMon,
  LOGF_Acquire,
  LOGF_Manual,
  LOGF_Keys,
  LOGF_Logging,
  LOGF_Nameserv,
  LOGF_Rtc,
  LOGF_Regress,
  LOGF_Sys,
  LOGF_SysGeneric,
  LOGF_SysLinux,
  LOGF_SysNetBSD,
  LOGF_SysSolaris,
  LOGF_SysSunOS,
  LOGF_SysWinnt,
  LOGF_TempComp,
  LOGF_RtcLinux,
  LOGF_Refclock,
  LOGF_Smooth,
} LOG_Facility;

/* Init function */
extern void LOG_Initialise(void);

/* Fini function */
extern void LOG_Finalise(void);

/* Line logging function */
FORMAT_ATTRIBUTE_PRINTF(6, 7)
extern void LOG_Message(LOG_Severity severity, LOG_Facility facility,
                        int line_number, const char *filename,
                        const char *function_name, const char *format, ...);

/* Set debug level:
   0, 1 - only non-debug messages are logged
   2    - debug messages are logged too, all messages are prefixed with
          filename, line, and function name
   */
extern void LOG_SetDebugLevel(int level);

/* Log messages to syslog instead of stderr */
extern void LOG_OpenSystemLog(void);

/* Send fatal message also to the foreground process */
extern void LOG_SetParentFd(int fd);

/* Close the pipe to the foreground process so it can exit */
extern void LOG_CloseParentFd(void);

/* File logging functions */

typedef int LOG_FileID;

extern LOG_FileID LOG_FileOpen(const char *name, const char *banner);

FORMAT_ATTRIBUTE_PRINTF(2, 3)
extern void LOG_FileWrite(LOG_FileID id, const char *format, ...);

extern void LOG_CreateLogFileDir(void);
extern void LOG_CycleLogFiles(void);

#endif /* GOT_LOGGING_H */
