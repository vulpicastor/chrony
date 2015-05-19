/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2003
 * Copyright (C) Miroslav Lichvar  2009-2015
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

  Module that reads and processes the configuration file.
  */

#include "config.h"

#include "sysincl.h"

#include "array.h"
#include "conf.h"
#include "ntp_sources.h"
#include "ntp_core.h"
#include "refclock.h"
#include "cmdmon.h"
#include "srcparams.h"
#include "logging.h"
#include "nameserv.h"
#include "memory.h"
#include "cmdparse.h"
#include "util.h"

/* ================================================== */
/* Forward prototypes */

static int parse_string(char *line, char **result);
static int parse_int(char *line, int *result);
static int parse_uint32(char *, uint32_t *result);
static int parse_double(char *line, double *result);
static int parse_null(char *line);

static void parse_allow(char *);
static void parse_bindacqaddress(char *);
static void parse_bindaddress(char *);
static void parse_bindcmdaddress(char *);
static void parse_broadcast(char *);
static void parse_clientloglimit(char *);
static void parse_cmdallow(char *);
static void parse_cmddeny(char *);
static void parse_deny(char *);
static void parse_fallbackdrift(char *);
static void parse_include(char *);
static void parse_initstepslew(char *);
static void parse_leapsecmode(char *);
static void parse_local(char *);
static void parse_log(char *);
static void parse_mailonchange(char *);
static void parse_makestep(char *);
static void parse_maxchange(char *);
static void parse_peer(char *);
static void parse_pool(char *);
static void parse_refclock(char *);
static void parse_server(char *);
static void parse_smoothtime(char *);
static void parse_tempcomp(char *);

/* ================================================== */
/* Configuration variables */

static int restarted = 0;
static int generate_command_key = 0;
static char *rtc_device;
static int acquisition_port = -1;
static int ntp_port = 123;
static char *keys_file = NULL;
static char *drift_file = NULL;
static char *rtc_file = NULL;
static uint32_t command_key_id;
static double max_update_skew = 1000.0;
static double correction_time_ratio = 3.0;
static double max_clock_error = 1.0; /* in ppm */
static double max_slew_rate = 1e6 / 12.0; /* in ppm */

static double reselect_distance = 1e-4;
static double stratum_weight = 1e-3;
static double combine_limit = 3.0;

static int cmd_port = DEFAULT_CANDM_PORT;

static int do_log_measurements = 0;
static int do_log_statistics = 0;
static int do_log_tracking = 0;
static int do_log_rtc = 0;
static int do_log_refclocks = 0;
static int do_log_tempcomp = 0;
static int do_dump_on_exit = 0;
static int log_banner = 32;
static char *logdir;
static char *dumpdir;

static int enable_local=0;
static int local_stratum;

/* Threshold (in seconds) - if absolute value of initial error is less
   than this, slew instead of stepping */
static double init_slew_threshold;
/* Array of IPAddr */
static ARR_Instance init_sources;

static int enable_manual=0;

/* Flag set if the RTC runs UTC (default is it runs local time
   incl. daylight saving). */
static int rtc_on_utc = 0;

/* Filename used to read the hwclock(8) LOCAL/UTC setting */
static char *hwclock_file = NULL;

/* Flag set if the RTC should be automatically synchronised by kernel */
static int rtc_sync = 0;

/* Limit and threshold for clock stepping */
static int make_step_limit = 0;
static double make_step_threshold = 0.0;

/* Threshold for automatic RTC trimming */
static double rtc_autotrim_threshold = 0.0;

/* Minimum number of selectables sources required to update the clock */
static int min_sources = 1;

/* Number of updates before offset checking, number of ignored updates
   before exiting and the maximum allowed offset */
static int max_offset_delay = -1;
static int max_offset_ignore;
static double max_offset;

/* Maximum and minimum number of samples per source */
static int max_samples = 0; /* no limit */
static int min_samples = 0;

/* Flag set if we should log to syslog when a time adjustment
   exceeding the threshold is initiated */
static int do_log_change = 0;
static double log_change_threshold = 0.0;

static char *mail_user_on_change = NULL;
static double mail_change_threshold = 0.0;

/* Flag indicating that we don't want to log clients, e.g. to save
   memory */
static int no_client_log = 0;

/* Limit memory allocated for the clients log */
static unsigned long client_log_limit = 524288;

/* Minimum and maximum fallback drift intervals */
static int fb_drift_min = 0;
static int fb_drift_max = 0;

/* IP addresses for binding the NTP server sockets to.  UNSPEC family means
   INADDR_ANY will be used */
static IPAddr bind_address4, bind_address6;

/* IP addresses for binding the NTP client sockets to.  UNSPEC family means
   INADDR_ANY will be used */
static IPAddr bind_acq_address4, bind_acq_address6;

/* IP addresses for binding the command socket to.  UNSPEC family means
   the loopback address will be used */
static IPAddr bind_cmd_address4, bind_cmd_address6;

/* Filename to use for storing pid of running chronyd, to prevent multiple
 * chronyds being started. */
static char *pidfile;

/* Smoothing constants */
static double smooth_max_freq = 0.0; /* in ppm */
static double smooth_max_wander = 0.0; /* in ppm/s */

/* Temperature sensor, update interval and compensation coefficients */
static char *tempcomp_sensor_file = NULL;
static char *tempcomp_point_file = NULL;
static double tempcomp_interval;
static double tempcomp_T0, tempcomp_k0, tempcomp_k1, tempcomp_k2;

static int sched_priority = 0;
static int lock_memory = 0;

/* Leap second handling mode */
static REF_LeapMode leapsec_mode = REF_LeapModeSystem;

/* Name of a system timezone containing leap seconds occuring at midnight */
static char *leapsec_tz = NULL;

/* Name of the user to which will be dropped root privileges. */
static char *user;

typedef struct {
  NTP_Source_Type type;
  int pool;
  CPS_NTP_Source params;
} NTP_Source;

/* Array of NTP_Source */
static ARR_Instance ntp_sources;

/* Array of RefclockParameters */
static ARR_Instance refclock_sources;

typedef struct _AllowDeny {
  IPAddr ip;
  int subnet_bits;
  int all; /* 1 to override existing more specific defns */
  int allow; /* 0 for deny, 1 for allow */
} AllowDeny;

/* Arrays of AllowDeny */
static ARR_Instance ntp_restrictions;
static ARR_Instance cmd_restrictions;

typedef struct {
  IPAddr addr;
  unsigned short port;
  int interval;
} NTP_Broadcast_Destination;

/* Array of NTP_Broadcast_Destination */
static ARR_Instance broadcasts;

/* ================================================== */

/* The line number in the configuration file being processed */
static int line_number;
static const char *processed_file;
static const char *processed_command;

/* ================================================== */

static void
command_parse_error(void)
{
    LOG_FATAL(LOGF_Configure, "Could not parse %s directive at line %d%s%s",
        processed_command, line_number, processed_file ? " in file " : "",
        processed_file ? processed_file : "");
}

/* ================================================== */

static void
other_parse_error(const char *message)
{
    LOG_FATAL(LOGF_Configure, "%s at line %d%s%s",
        message, line_number, processed_file ? " in file " : "",
        processed_file ? processed_file : "");
}

/* ================================================== */

static int
get_number_of_args(char *line)
{
  int num = 0;

  /* The line is normalized, between arguments is just one space */
  if (*line == ' ')
    line++;
  if (*line)
    num++;
  for (; *line; line++) {
    if (*line == ' ')
      num++;
  }

  return num;
}

/* ================================================== */

static void
check_number_of_args(char *line, int num)
{
  num -= get_number_of_args(line);

  if (num) {
    LOG_FATAL(LOGF_Configure, "%s arguments for %s directive at line %d%s%s",
        num > 0 ? "Missing" : "Too many",
        processed_command, line_number, processed_file ? " in file " : "",
        processed_file ? processed_file : "");
  }
}

/* ================================================== */

void
CNF_Initialise(int r)
{
  restarted = r;

  init_sources = ARR_CreateInstance(sizeof (IPAddr));
  ntp_sources = ARR_CreateInstance(sizeof (NTP_Source));
  refclock_sources = ARR_CreateInstance(sizeof (RefclockParameters));
  broadcasts = ARR_CreateInstance(sizeof (NTP_Broadcast_Destination));

  ntp_restrictions = ARR_CreateInstance(sizeof (AllowDeny));
  cmd_restrictions = ARR_CreateInstance(sizeof (AllowDeny));

  dumpdir = Strdup(".");
  logdir = Strdup(".");
  pidfile = Strdup("/var/run/chronyd.pid");
  rtc_device = Strdup("/dev/rtc");
  user = Strdup(DEFAULT_USER);
}

/* ================================================== */

void
CNF_Finalise(void)
{
  unsigned int i;

  for (i = 0; i < ARR_GetSize(ntp_sources); i++)
    Free(((NTP_Source *)ARR_GetElement(ntp_sources, i))->params.name);

  ARR_DestroyInstance(init_sources);
  ARR_DestroyInstance(ntp_sources);
  ARR_DestroyInstance(refclock_sources);
  ARR_DestroyInstance(broadcasts);

  ARR_DestroyInstance(ntp_restrictions);
  ARR_DestroyInstance(cmd_restrictions);

  Free(drift_file);
  Free(dumpdir);
  Free(hwclock_file);
  Free(keys_file);
  Free(leapsec_tz);
  Free(logdir);
  Free(pidfile);
  Free(rtc_device);
  Free(rtc_file);
  Free(user);
  Free(mail_user_on_change);
  Free(tempcomp_sensor_file);
  Free(tempcomp_point_file);
}

/* ================================================== */

/* Read the configuration file */
void
CNF_ReadFile(const char *filename)
{
  FILE *in;
  char line[2048];
  int i;

  in = fopen(filename, "r");
  if (!in) {
    LOG_FATAL(LOGF_Configure, "Could not open configuration file %s", filename);
    return;
  }

  for (i = 1; fgets(line, sizeof(line), in); i++) {
    CNF_ParseLine(filename, i, line);
  }

  fclose(in);
}

/* ================================================== */

/* Parse one configuration line */
void
CNF_ParseLine(const char *filename, int number, char *line)
{
  char *p, *command;

  /* Set global variables used in error messages */
  processed_file = filename;
  line_number = number;

  /* Remove extra white-space and comments */
  CPS_NormalizeLine(line);

  /* Skip blank lines */
  if (!*line)
    return;

  /* We have a real line, now try to match commands */
  processed_command = command = line;
  p = CPS_SplitWord(line);

  if (!strcasecmp(command, "acquisitionport")) {
    parse_int(p, &acquisition_port);
  } else if (!strcasecmp(command, "allow")) {
    parse_allow(p);
  } else if (!strcasecmp(command, "bindacqaddress")) {
    parse_bindacqaddress(p);
  } else if (!strcasecmp(command, "bindaddress")) {
    parse_bindaddress(p);
  } else if (!strcasecmp(command, "bindcmdaddress")) {
    parse_bindcmdaddress(p);
  } else if (!strcasecmp(command, "broadcast")) {
    parse_broadcast(p);
  } else if (!strcasecmp(command, "clientloglimit")) {
    parse_clientloglimit(p);
  } else if (!strcasecmp(command, "cmdallow")) {
    parse_cmdallow(p);
  } else if (!strcasecmp(command, "cmddeny")) {
    parse_cmddeny(p);
  } else if (!strcasecmp(command, "cmdport")) {
    parse_int(p, &cmd_port);
  } else if (!strcasecmp(command, "combinelimit")) {
    parse_double(p, &combine_limit);
  } else if (!strcasecmp(command, "commandkey")) {
    parse_uint32(p, &command_key_id);
  } else if (!strcasecmp(command, "corrtimeratio")) {
    parse_double(p, &correction_time_ratio);
  } else if (!strcasecmp(command, "deny")) {
    parse_deny(p);
  } else if (!strcasecmp(command, "driftfile")) {
    parse_string(p, &drift_file);
  } else if (!strcasecmp(command, "dumpdir")) {
    parse_string(p, &dumpdir);
  } else if (!strcasecmp(command, "dumponexit")) {
    do_dump_on_exit = parse_null(p);
  } else if (!strcasecmp(command, "fallbackdrift")) {
    parse_fallbackdrift(p);
  } else if (!strcasecmp(command, "generatecommandkey")) {
    generate_command_key = parse_null(p);
  } else if (!strcasecmp(command, "hwclockfile")) {
    parse_string(p, &hwclock_file);
  } else if (!strcasecmp(command, "include")) {
    parse_include(p);
  } else if (!strcasecmp(command, "initstepslew")) {
    parse_initstepslew(p);
  } else if (!strcasecmp(command, "keyfile")) {
    parse_string(p, &keys_file);
  } else if (!strcasecmp(command, "leapsecmode")) {
    parse_leapsecmode(p);
  } else if (!strcasecmp(command, "leapsectz")) {
    parse_string(p, &leapsec_tz);
  } else if (!strcasecmp(command, "linux_freq_scale")) {
    LOG(LOGS_WARN, LOGF_Configure, "%s directive is no longer supported", command);
  } else if (!strcasecmp(command, "linux_hz")) {
    LOG(LOGS_WARN, LOGF_Configure, "%s directive is no longer supported", command);
  } else if (!strcasecmp(command, "local")) {
    parse_local(p);
  } else if (!strcasecmp(command, "lock_all")) {
    lock_memory = parse_null(p);
  } else if (!strcasecmp(command, "log")) {
    parse_log(p);
  } else if (!strcasecmp(command, "logbanner")) {
    parse_int(p, &log_banner);
  } else if (!strcasecmp(command, "logchange")) {
    do_log_change = parse_double(p, &log_change_threshold);
  } else if (!strcasecmp(command, "logdir")) {
    parse_string(p, &logdir);
  } else if (!strcasecmp(command, "mailonchange")) {
    parse_mailonchange(p);
  } else if (!strcasecmp(command, "makestep")) {
    parse_makestep(p);
  } else if (!strcasecmp(command, "manual")) {
    enable_manual = parse_null(p);
  } else if (!strcasecmp(command, "maxchange")) {
    parse_maxchange(p);
  } else if (!strcasecmp(command, "maxclockerror")) {
    parse_double(p, &max_clock_error);
  } else if (!strcasecmp(command, "maxsamples")) {
    parse_int(p, &max_samples);
  } else if (!strcasecmp(command, "maxslewrate")) {
    parse_double(p, &max_slew_rate);
  } else if (!strcasecmp(command, "maxupdateskew")) {
    parse_double(p, &max_update_skew);
  } else if (!strcasecmp(command, "minsamples")) {
    parse_int(p, &min_samples);
  } else if (!strcasecmp(command, "minsources")) {
    parse_int(p, &min_sources);
  } else if (!strcasecmp(command, "noclientlog")) {
    no_client_log = parse_null(p);
  } else if (!strcasecmp(command, "peer")) {
    parse_peer(p);
  } else if (!strcasecmp(command, "pidfile")) {
    parse_string(p, &pidfile);
  } else if (!strcasecmp(command, "pool")) {
    parse_pool(p);
  } else if (!strcasecmp(command, "port")) {
    parse_int(p, &ntp_port);
  } else if (!strcasecmp(command, "refclock")) {
    parse_refclock(p);
  } else if (!strcasecmp(command, "reselectdist")) {
    parse_double(p, &reselect_distance);
  } else if (!strcasecmp(command, "rtcautotrim")) {
    parse_double(p, &rtc_autotrim_threshold);
  } else if (!strcasecmp(command, "rtcdevice")) {
    parse_string(p, &rtc_device);
  } else if (!strcasecmp(command, "rtcfile")) {
    parse_string(p, &rtc_file);
  } else if (!strcasecmp(command, "rtconutc")) {
    rtc_on_utc = parse_null(p);
  } else if (!strcasecmp(command, "rtcsync")) {
    rtc_sync = parse_null(p);
  } else if (!strcasecmp(command, "sched_priority")) {
    parse_int(p, &sched_priority);
  } else if (!strcasecmp(command, "server")) {
    parse_server(p);
  } else if (!strcasecmp(command, "smoothtime")) {
    parse_smoothtime(p);
  } else if (!strcasecmp(command, "stratumweight")) {
    parse_double(p, &stratum_weight);
  } else if (!strcasecmp(command, "tempcomp")) {
    parse_tempcomp(p);
  } else if (!strcasecmp(command, "user")) {
    parse_string(p, &user);
  } else {
    other_parse_error("Invalid command");
  }
}

/* ================================================== */

static int
parse_string(char *line, char **result)
{
  check_number_of_args(line, 1);
  Free(*result);
  *result = Strdup(line);
  return 1;
}

/* ================================================== */

static int
parse_int(char *line, int *result)
{
  check_number_of_args(line, 1);
  if (sscanf(line, "%d", result) != 1) {
    command_parse_error();
    return 0;
  }
  return 1;
}

/* ================================================== */

static int
parse_uint32(char *line, uint32_t *result)
{
  check_number_of_args(line, 1);
  if (sscanf(line, "%"SCNu32, result) != 1) {
    command_parse_error();
    return 0;
  }
  return 1;
}

/* ================================================== */

static int
parse_double(char *line, double *result)
{
  check_number_of_args(line, 1);
  if (sscanf(line, "%lf", result) != 1) {
    command_parse_error();
    return 0;
  }
  return 1;
}

/* ================================================== */

static int
parse_null(char *line)
{
  check_number_of_args(line, 0);
  return 1;
}

/* ================================================== */

static void
parse_source(char *line, NTP_Source_Type type, int pool)
{
  CPS_Status status;
  NTP_Source source;
  char str[64];

  source.type = type;
  source.pool = pool;
  status = CPS_ParseNTPSourceAdd(line, &source.params);

  if (status != CPS_Success) {
    CPS_StatusToString(status, str, sizeof (str));
    other_parse_error(str);
    return;
  }

  source.params.name = Strdup(source.params.name);
  ARR_AppendElement(ntp_sources, &source);
}

/* ================================================== */

static void
parse_server(char *line)
{
  parse_source(line, NTP_SERVER, 0);
}

/* ================================================== */

static void
parse_peer(char *line)
{
  parse_source(line, NTP_PEER, 0);
}

/* ================================================== */

static void
parse_pool(char *line)
{
  parse_source(line, NTP_SERVER, 1);
}

/* ================================================== */

static void
parse_refclock(char *line)
{
  int n, poll, dpoll, filter_length, pps_rate, min_samples, max_samples;
  uint32_t ref_id, lock_ref_id;
  double offset, delay, precision, max_dispersion;
  char *p, *cmd, *name, *param;
  unsigned char ref[5];
  SRC_SelectOption sel_option;
  RefclockParameters *refclock;

  poll = 4;
  dpoll = 0;
  filter_length = 64;
  pps_rate = 0;
  min_samples = SRC_DEFAULT_MINSAMPLES;
  max_samples = SRC_DEFAULT_MAXSAMPLES;
  offset = 0.0;
  delay = 1e-9;
  precision = 0.0;
  max_dispersion = 0.0;
  ref_id = 0;
  lock_ref_id = 0;
  sel_option = SRC_SelectNormal;

  if (!*line) {
    command_parse_error();
    return;
  }

  p = line;
  line = CPS_SplitWord(line);

  if (!*line) {
    command_parse_error();
    return;
  }

  name = Strdup(p);

  p = line;
  line = CPS_SplitWord(line);
  param = Strdup(p);

  while (*line) {
    cmd = line;
    line = CPS_SplitWord(line);
    if (!strcasecmp(cmd, "refid")) {
      if (sscanf(line, "%4s%n", (char *)ref, &n) != 1)
        break;
      ref_id = ref[0] << 24 | ref[1] << 16 | ref[2] << 8 | ref[3];
    } else if (!strcasecmp(cmd, "lock")) {
      if (sscanf(line, "%4s%n", (char *)ref, &n) != 1)
        break;
      lock_ref_id = ref[0] << 24 | ref[1] << 16 | ref[2] << 8 | ref[3];
    } else if (!strcasecmp(cmd, "poll")) {
      if (sscanf(line, "%d%n", &poll, &n) != 1) {
        break;
      }
    } else if (!strcasecmp(cmd, "dpoll")) {
      if (sscanf(line, "%d%n", &dpoll, &n) != 1) {
        break;
      }
    } else if (!strcasecmp(cmd, "filter")) {
      if (sscanf(line, "%d%n", &filter_length, &n) != 1) {
        break;
      }
    } else if (!strcasecmp(cmd, "rate")) {
      if (sscanf(line, "%d%n", &pps_rate, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "minsamples")) {
      if (sscanf(line, "%d%n", &min_samples, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "maxsamples")) {
      if (sscanf(line, "%d%n", &max_samples, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "offset")) {
      if (sscanf(line, "%lf%n", &offset, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "delay")) {
      if (sscanf(line, "%lf%n", &delay, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "precision")) {
      if (sscanf(line, "%lf%n", &precision, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "maxdispersion")) {
      if (sscanf(line, "%lf%n", &max_dispersion, &n) != 1)
        break;
    } else if (!strcasecmp(cmd, "noselect")) {
      n = 0;
      sel_option = SRC_SelectNoselect;
    } else if (!strcasecmp(cmd, "prefer")) {
      n = 0;
      sel_option = SRC_SelectPrefer;
    } else {
      break;
    }
    line += n;
  }

  if (*line) {
    other_parse_error("Invalid/unreadable refclock parameter");
    return;
  }

  refclock = (RefclockParameters *)ARR_GetNewElement(refclock_sources);
  refclock->driver_name = name;
  refclock->driver_parameter = param;
  refclock->driver_poll = dpoll;
  refclock->poll = poll;
  refclock->filter_length = filter_length;
  refclock->pps_rate = pps_rate;
  refclock->min_samples = min_samples;
  refclock->max_samples = max_samples;
  refclock->offset = offset;
  refclock->delay = delay;
  refclock->precision = precision;
  refclock->max_dispersion = max_dispersion;
  refclock->sel_option = sel_option;
  refclock->ref_id = ref_id;
  refclock->lock_ref_id = lock_ref_id;
}

/* ================================================== */

static void
parse_log(char *line)
{
  char *log_name;
  do {
    log_name = line;
    line = CPS_SplitWord(line);
    if (*log_name) {
      if (!strcmp(log_name, "measurements")) {
        do_log_measurements = 1;
      } else if (!strcmp(log_name, "statistics")) {
        do_log_statistics = 1;
      } else if (!strcmp(log_name, "tracking")) {
        do_log_tracking = 1;
      } else if (!strcmp(log_name, "rtc")) {
        do_log_rtc = 1;
      } else if (!strcmp(log_name, "refclocks")) {
        do_log_refclocks = 1;
      } else if (!strcmp(log_name, "tempcomp")) {
        do_log_tempcomp = 1;
      } else {
        other_parse_error("Invalid log parameter");
        break;
      }
    } else {
      break;
    }
  } while (1);
}

/* ================================================== */

static void
parse_local(char *line)
{
  int stratum;
  if (sscanf(line, "stratum%d", &stratum) == 1) {
    local_stratum = stratum;
    enable_local = 1;
  } else {
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_initstepslew(char *line)
{
  char *p, *hostname;
  IPAddr ip_addr;

  /* Ignore the line if chronyd was started with -R. */
  if (restarted) {
    return;
  }

  ARR_SetSize(init_sources, 0);
  p = CPS_SplitWord(line);

  if (sscanf(line, "%lf", &init_slew_threshold) != 1) {
    command_parse_error();
    return;
  }

  while (*p) {
    hostname = p;
    p = CPS_SplitWord(p);
    if (*hostname) {
      if (DNS_Name2IPAddress(hostname, &ip_addr, 1) == DNS_Success) {
        ARR_AppendElement(init_sources, &ip_addr);
      } else {
        LOG(LOGS_WARN, LOGF_Configure, "Could not resolve address of initstepslew server %s", hostname);
      }
    }
  }
}

/* ================================================== */

static void
parse_leapsecmode(char *line)
{
  if (!strcasecmp(line, "system"))
    leapsec_mode = REF_LeapModeSystem;
  else if (!strcasecmp(line, "slew"))
    leapsec_mode = REF_LeapModeSlew;
  else if (!strcasecmp(line, "step"))
    leapsec_mode = REF_LeapModeStep;
  else if (!strcasecmp(line, "ignore"))
    leapsec_mode = REF_LeapModeIgnore;
  else
    command_parse_error();
}

/* ================================================== */

static void
parse_clientloglimit(char *line)
{
  check_number_of_args(line, 1);
  if (sscanf(line, "%lu", &client_log_limit) != 1) {
    command_parse_error();
  }

  if (client_log_limit == 0) {
    /* unlimited */
    client_log_limit = (unsigned long)-1;
  }
}

/* ================================================== */

static void
parse_fallbackdrift(char *line)
{
  check_number_of_args(line, 2);
  if (sscanf(line, "%d %d", &fb_drift_min, &fb_drift_max) != 2) {
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_makestep(char *line)
{
  check_number_of_args(line, 2);
  if (sscanf(line, "%lf %d", &make_step_threshold, &make_step_limit) != 2) {
    make_step_limit = 0;
    command_parse_error();
  }

  /* Disable limited makestep if chronyd was started with -R. */
  if (restarted && make_step_limit > 0) {
    make_step_limit = 0;
  }
}

/* ================================================== */

static void
parse_maxchange(char *line)
{
  check_number_of_args(line, 3);
  if (sscanf(line, "%lf %d %d", &max_offset, &max_offset_delay, &max_offset_ignore) != 3) {
    max_offset_delay = -1;
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_mailonchange(char *line)
{
  char *address;
  check_number_of_args(line, 2);
  address = line;
  line = CPS_SplitWord(line);
  Free(mail_user_on_change);
  if (sscanf(line, "%lf", &mail_change_threshold) == 1) {
    mail_user_on_change = Strdup(address);
  } else {
    mail_user_on_change = NULL;
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_allow_deny(char *line, ARR_Instance restrictions, int allow)
{
  char *p;
  unsigned long a, b, c, d, n;
  int all = 0;
  AllowDeny *new_node = NULL;
  IPAddr ip_addr;

  p = line;

  if (!strncmp(p, "all", 3)) {
    all = 1;
    p = CPS_SplitWord(line);
  }

  if (!*p) {
    /* Empty line applies to all addresses */
    new_node = (AllowDeny *)ARR_GetNewElement(restrictions);
    new_node->allow = allow;
    new_node->all = all;
    new_node->ip.family = IPADDR_UNSPEC;
    new_node->subnet_bits = 0;
  } else {
    char *slashpos;
    slashpos = strchr(p, '/');
    if (slashpos) *slashpos = 0;

    check_number_of_args(p, 1);
    n = 0;
    if (UTI_StringToIP(p, &ip_addr) ||
        (n = sscanf(p, "%lu.%lu.%lu.%lu", &a, &b, &c, &d)) >= 1) {
      new_node = (AllowDeny *)ARR_GetNewElement(restrictions);
      new_node->allow = allow;
      new_node->all = all;

      if (n == 0) {
        new_node->ip = ip_addr;
        if (ip_addr.family == IPADDR_INET6)
          new_node->subnet_bits = 128;
        else
          new_node->subnet_bits = 32;
      } else {
        new_node->ip.family = IPADDR_INET4;

        a &= 0xff;
        b &= 0xff;
        c &= 0xff;
        d &= 0xff;
        
        switch (n) {
          case 1:
            new_node->ip.addr.in4 = (a<<24);
            new_node->subnet_bits = 8;
            break;
          case 2:
            new_node->ip.addr.in4 = (a<<24) | (b<<16);
            new_node->subnet_bits = 16;
            break;
          case 3:
            new_node->ip.addr.in4 = (a<<24) | (b<<16) | (c<<8);
            new_node->subnet_bits = 24;
            break;
          case 4:
            new_node->ip.addr.in4 = (a<<24) | (b<<16) | (c<<8) | d;
            new_node->subnet_bits = 32;
            break;
          default:
            assert(0);
        }
      }
      
      if (slashpos) {
        int specified_subnet_bits, n;
        n = sscanf(slashpos+1, "%d", &specified_subnet_bits);
        if (n == 1) {
          new_node->subnet_bits = specified_subnet_bits;
        } else {
          command_parse_error();
        }
      }

    } else {
      if (DNS_Name2IPAddress(p, &ip_addr, 1) == DNS_Success) {
        new_node = (AllowDeny *)ARR_GetNewElement(restrictions);
        new_node->allow = allow;
        new_node->all = all;
        new_node->ip = ip_addr;
        if (ip_addr.family == IPADDR_INET6)
          new_node->subnet_bits = 128;
        else
          new_node->subnet_bits = 32;
      } else {
        command_parse_error();
      }      
    }
  }
}
  

/* ================================================== */

static void
parse_allow(char *line)
{
  parse_allow_deny(line, ntp_restrictions, 1);
}


/* ================================================== */

static void
parse_deny(char *line)
{
  parse_allow_deny(line, ntp_restrictions, 0);
}

/* ================================================== */

static void
parse_cmdallow(char *line)
{
  parse_allow_deny(line, cmd_restrictions, 1);
}


/* ================================================== */

static void
parse_cmddeny(char *line)
{
  parse_allow_deny(line, cmd_restrictions, 0);
}

/* ================================================== */

static void
parse_bindacqaddress(char *line)
{
  IPAddr ip;

  check_number_of_args(line, 1);
  if (UTI_StringToIP(line, &ip)) {
    if (ip.family == IPADDR_INET4)
      bind_acq_address4 = ip;
    else if (ip.family == IPADDR_INET6)
      bind_acq_address6 = ip;
  } else {
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_bindaddress(char *line)
{
  IPAddr ip;

  check_number_of_args(line, 1);
  if (UTI_StringToIP(line, &ip)) {
    if (ip.family == IPADDR_INET4)
      bind_address4 = ip;
    else if (ip.family == IPADDR_INET6)
      bind_address6 = ip;
  } else {
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_bindcmdaddress(char *line)
{
  IPAddr ip;

  check_number_of_args(line, 1);
  if (UTI_StringToIP(line, &ip)) {
    if (ip.family == IPADDR_INET4)
      bind_cmd_address4 = ip;
    else if (ip.family == IPADDR_INET6)
      bind_cmd_address6 = ip;
  } else {
    command_parse_error();
  }
}

/* ================================================== */

static void
parse_broadcast(char *line)
{
  /* Syntax : broadcast <interval> <broadcast-IP-addr> [<port>] */
  NTP_Broadcast_Destination *destination;
  int port;
  int interval;
  char *p;
  IPAddr ip;
  
  p = line;
  line = CPS_SplitWord(line);

  if (sscanf(p, "%d", &interval) != 1) {
    command_parse_error();
    return;
  }

  p = line;
  line = CPS_SplitWord(line);

  if (!UTI_StringToIP(p, &ip)) {
    command_parse_error();
    return;
  }

  p = line;
  line = CPS_SplitWord(line);

  if (*p) {
    if (sscanf(p, "%d", &port) != 1 || *line) {
      command_parse_error();
      return;
    }
  } else {
    /* default port */
    port = 123;
  }

  destination = (NTP_Broadcast_Destination *)ARR_GetNewElement(broadcasts);
  destination->addr = ip;
  destination->port = port;
  destination->interval = interval;
}

/* ================================================== */

static void
parse_smoothtime(char *line)
{
  check_number_of_args(line, 2);
  if (sscanf(line, "%lf %lf", &smooth_max_freq, &smooth_max_wander) != 2) {
    smooth_max_freq = 0.0;
    command_parse_error();
  }
}

/* ================================================== */
static void
parse_tempcomp(char *line)
{
  char *p;
  int point_form;

  point_form = get_number_of_args(line) == 3;

  if (!point_form)
    check_number_of_args(line, 6);

  p = line;
  line = CPS_SplitWord(line);

  if (!*p) {
    command_parse_error();
    return;
  }

  Free(tempcomp_point_file);

  if (point_form) {
    if (sscanf(line, "%lf", &tempcomp_interval) != 1) {
      command_parse_error();
      return;
    }
    tempcomp_point_file = Strdup(CPS_SplitWord(line));
  } else {
    if (sscanf(line, "%lf %lf %lf %lf %lf", &tempcomp_interval,
               &tempcomp_T0, &tempcomp_k0, &tempcomp_k1, &tempcomp_k2) != 5) {
      command_parse_error();
      return;
    }
    tempcomp_point_file = NULL;
  }

  Free(tempcomp_sensor_file);
  tempcomp_sensor_file = Strdup(p);
}

/* ================================================== */

static void
parse_include(char *line)
{
  check_number_of_args(line, 1);
  CNF_ReadFile(line);
}

/* ================================================== */

void
CNF_AddInitSources(void)
{
  CPS_NTP_Source cps_source;
  NTP_Remote_Address ntp_addr;
  char dummy_hostname[2] = "H";
  unsigned int i;

  for (i = 0; i < ARR_GetSize(init_sources); i++) {
    /* Get the default NTP params */
    CPS_ParseNTPSourceAdd(dummy_hostname, &cps_source);

    /* Add the address as an offline iburst server */
    ntp_addr.ip_addr = *(IPAddr *)ARR_GetElement(init_sources, i);
    ntp_addr.port = cps_source.port;
    cps_source.params.iburst = 1;
    cps_source.params.online = 0;

    NSR_AddSource(&ntp_addr, NTP_SERVER, &cps_source.params);
  }

  ARR_SetSize(init_sources, 0);
}

/* ================================================== */

void
CNF_AddSources(void)
{
  NTP_Source *source;
  unsigned int i;

  for (i = 0; i < ARR_GetSize(ntp_sources); i++) {
    source = (NTP_Source *)ARR_GetElement(ntp_sources, i);
    NSR_AddSourceByName(source->params.name, source->params.port,
                        source->pool, source->type, &source->params.params);
    Free(source->params.name);
  }

  ARR_SetSize(ntp_sources, 0);
}

/* ================================================== */

void
CNF_AddRefclocks(void)
{
  unsigned int i;

  for (i = 0; i < ARR_GetSize(refclock_sources); i++) {
    RCL_AddRefclock((RefclockParameters *)ARR_GetElement(refclock_sources, i));
  }

  ARR_SetSize(refclock_sources, 0);
}

/* ================================================== */

void
CNF_AddBroadcasts(void)
{
  unsigned int i;
  NTP_Broadcast_Destination *destination;

  for (i = 0; i < ARR_GetSize(broadcasts); i++) {
    destination = (NTP_Broadcast_Destination *)ARR_GetElement(broadcasts, i);
    NCR_AddBroadcastDestination(&destination->addr, destination->port,
                                destination->interval);
  }

  ARR_SetSize(broadcasts, 0);
}

/* ================================================== */

int
CNF_GetNTPPort(void)
{
  return ntp_port;
}

/* ================================================== */

int
CNF_GetAcquisitionPort(void)
{
  return acquisition_port;
}

/* ================================================== */

char *
CNF_GetDriftFile(void)
{
  return drift_file;
}

/* ================================================== */

int
CNF_GetLogBanner(void)
{
  return log_banner;
}

/* ================================================== */

char *
CNF_GetLogDir(void)
{
  return logdir;
}

/* ================================================== */

char *
CNF_GetDumpDir(void)
{
  return dumpdir;
}

/* ================================================== */

int
CNF_GetLogMeasurements(void)
{
  return do_log_measurements;
}

/* ================================================== */

int
CNF_GetLogStatistics(void)
{
  return do_log_statistics;
}

/* ================================================== */

int
CNF_GetLogTracking(void)
{
  return do_log_tracking;
}

/* ================================================== */

int
CNF_GetLogRtc(void)
{
  return do_log_rtc;
}

/* ================================================== */

int
CNF_GetLogRefclocks(void)
{
  return do_log_refclocks;
}

/* ================================================== */

int
CNF_GetLogTempComp(void)
{
  return do_log_tempcomp;
}

/* ================================================== */

char *
CNF_GetKeysFile(void)
{
  return keys_file;
}

/* ================================================== */

double
CNF_GetRtcAutotrim(void)
{
  return rtc_autotrim_threshold;
}

/* ================================================== */

char *
CNF_GetRtcFile(void)
{
  return rtc_file;
}

/* ================================================== */

char *
CNF_GetRtcDevice(void)
{
  return rtc_device;
}

/* ================================================== */

uint32_t
CNF_GetCommandKey(void)
{
  return command_key_id;
}

/* ================================================== */

int
CNF_GetGenerateCommandKey(void)
{
  return generate_command_key;
}

/* ================================================== */

int
CNF_GetDumpOnExit(void)
{
  return do_dump_on_exit;
}

/* ================================================== */

double
CNF_GetMaxUpdateSkew(void)
{
  return max_update_skew;
}

/* ================================================== */

double
CNF_GetMaxClockError(void)
{
  return max_clock_error;
}

/* ================================================== */

double
CNF_GetCorrectionTimeRatio(void)
{
  return correction_time_ratio;
}

/* ================================================== */

double
CNF_GetMaxSlewRate(void)
{
  return max_slew_rate;
}

/* ================================================== */

double
CNF_GetReselectDistance(void)
{
  return reselect_distance;
}

/* ================================================== */

double
CNF_GetStratumWeight(void)
{
  return stratum_weight;
}

/* ================================================== */

double
CNF_GetCombineLimit(void)
{
  return combine_limit;
}

/* ================================================== */

int
CNF_GetManualEnabled(void)
{
  return enable_manual;
}

/* ================================================== */

int
CNF_GetCommandPort(void) {
  return cmd_port;
}

/* ================================================== */

int
CNF_AllowLocalReference(int *stratum)
{
  if (enable_local) {
    *stratum = local_stratum;
    return 1;
  } else {
    return 0;
  }
}

/* ================================================== */

int
CNF_GetRtcOnUtc(void)
{
  return rtc_on_utc;
}

/* ================================================== */

int
CNF_GetRtcSync(void)
{
  return rtc_sync;
}

/* ================================================== */

void
CNF_GetMakeStep(int *limit, double *threshold)
{
  *limit = make_step_limit;
  *threshold = make_step_threshold;
}

/* ================================================== */

void
CNF_GetMaxChange(int *delay, int *ignore, double *offset)
{
  *delay = max_offset_delay;
  *ignore = max_offset_ignore;
  *offset = max_offset;
}

/* ================================================== */

void
CNF_GetLogChange(int *enabled, double *threshold)
{
  *enabled = do_log_change;
  *threshold = log_change_threshold;
}

/* ================================================== */

void
CNF_GetMailOnChange(int *enabled, double *threshold, char **user)
{
  if (mail_user_on_change) {
    *enabled = 1;
    *threshold = mail_change_threshold;
    *user = mail_user_on_change;
  } else {
    *enabled = 0;
    *threshold = 0.0;
    *user = NULL;
  }
}  

/* ================================================== */

void
CNF_SetupAccessRestrictions(void)
{
  AllowDeny *node;
  int status;
  unsigned int i;

  for (i = 0; i < ARR_GetSize(ntp_restrictions); i++) {
    node = ARR_GetElement(ntp_restrictions, i);
    status = NCR_AddAccessRestriction(&node->ip, node->subnet_bits, node->allow, node->all);
    if (!status) {
      LOG_FATAL(LOGF_Configure, "Bad subnet in %s/%d", UTI_IPToString(&node->ip), node->subnet_bits);
    }
  }

  for (i = 0; i < ARR_GetSize(cmd_restrictions); i++) {
    node = ARR_GetElement(cmd_restrictions, i);
    status = CAM_AddAccessRestriction(&node->ip, node->subnet_bits, node->allow, node->all);
    if (!status) {
      LOG_FATAL(LOGF_Configure, "Bad subnet in %s/%d", UTI_IPToString(&node->ip), node->subnet_bits);
    }
  }

  ARR_SetSize(ntp_restrictions, 0);
  ARR_SetSize(cmd_restrictions, 0);
}

/* ================================================== */

int
CNF_GetNoClientLog(void)
{
  return no_client_log;
}

/* ================================================== */

unsigned long
CNF_GetClientLogLimit(void)
{
  return client_log_limit;
}

/* ================================================== */

void
CNF_GetFallbackDrifts(int *min, int *max)
{
  *min = fb_drift_min;
  *max = fb_drift_max;
}

/* ================================================== */

void
CNF_GetBindAddress(int family, IPAddr *addr)
{
  if (family == IPADDR_INET4)
    *addr = bind_address4;
  else if (family == IPADDR_INET6)
    *addr = bind_address6;
  else
    addr->family = IPADDR_UNSPEC;
}

/* ================================================== */

void
CNF_GetBindAcquisitionAddress(int family, IPAddr *addr)
{
  if (family == IPADDR_INET4)
    *addr = bind_acq_address4;
  else if (family == IPADDR_INET6)
    *addr = bind_acq_address6;
  else
    addr->family = IPADDR_UNSPEC;
}

/* ================================================== */

void
CNF_GetBindCommandAddress(int family, IPAddr *addr)
{
  if (family == IPADDR_INET4)
    *addr = bind_cmd_address4;
  else if (family == IPADDR_INET6)
    *addr = bind_cmd_address6;
  else
    addr->family = IPADDR_UNSPEC;
}

/* ================================================== */

char *
CNF_GetPidFile(void)
{
  return pidfile;
}

/* ================================================== */

REF_LeapMode
CNF_GetLeapSecMode(void)
{
  return leapsec_mode;
}

/* ================================================== */

char *
CNF_GetLeapSecTimezone(void)
{
  return leapsec_tz;
}

/* ================================================== */

int
CNF_GetSchedPriority(void)
{
  return sched_priority;
}

/* ================================================== */

int
CNF_GetLockMemory(void)
{
  return lock_memory;
}

/* ================================================== */

void
CNF_GetSmooth(double *max_freq, double *max_wander)
{
  *max_freq = smooth_max_freq;
  *max_wander = smooth_max_wander;
}

/* ================================================== */

void
CNF_GetTempComp(char **file, double *interval, char **point_file, double *T0, double *k0, double *k1, double *k2)
{
  *file = tempcomp_sensor_file;
  *point_file = tempcomp_point_file;
  *interval = tempcomp_interval;
  *T0 = tempcomp_T0;
  *k0 = tempcomp_k0;
  *k1 = tempcomp_k1;
  *k2 = tempcomp_k2;
}

/* ================================================== */

char *
CNF_GetUser(void)
{
  return user;
}

/* ================================================== */

int
CNF_GetMaxSamples(void)
{
  return max_samples;
}

/* ================================================== */

int
CNF_GetMinSamples(void)
{
  return min_samples;
}

/* ================================================== */

int
CNF_GetMinSources(void)
{
  return min_sources;
}

/* ================================================== */

char *
CNF_GetHwclockFile(void)
{
  return hwclock_file;
}

/* ================================================== */

int
CNF_GetInitSources(void)
{
  return ARR_GetSize(init_sources);
}

/* ================================================== */

double
CNF_GetInitStepThreshold(void)
{
  return init_slew_threshold;
}
