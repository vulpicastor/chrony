/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2003
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

  This file includes all system header files that the software
  requires.  This allows us to isolate system dependencies to this file
  alone.
  */

#ifndef GOT_SYSINCL_H
#define GOT_SYSINCL_H

#if defined (SOLARIS) || defined(SUNOS) || defined(LINUX) || defined(__NetBSD__)

#if !defined(__NetBSD__) && !defined(__FreeBSD__)
#include <alloca.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#if !defined(__FreeBSD__)
#include <malloc.h>
#endif
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <syslog.h>
#include <time.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#else
/* Tough */
#endif

/* One or other of these to make getsid() visible */
#define __EXTENSIONS__ 1
#define __USE_XOPEN_EXTENDED 1

#include <unistd.h>

#endif

#ifdef FEAT_IPV6
/* For inet_ntop() */
#include <arpa/inet.h>
#endif

#if defined (SOLARIS) || defined(SUNOS)
/* Only needed on these platforms, and doesn't exist on some Linux
   versions. */
#include <nlist.h>
#endif

#if defined (WINNT)

/* Designed to work with the GCC from the GNAT-3.10 for Win32
   distribution */

#define Win32_Winsock
#include <assert.h>
#include <ctype.h>

#if 1
/* Cheat and inline the necessary bits from <errno.h>.  We don't
   include it directly because it redefines some EXXX constants that
   conflict with <windows32/sockets.h> (included by <windows.h>) */

int*	_errno();
int*	__doserrno();

#define	errno		(*_errno())
#define	_doserrno	(*__doserrno())

#define ENOENT 2
#else

#include <errno.h>
#endif


#include <float.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>
#endif

#endif /* GOT_SYSINCL_H */
