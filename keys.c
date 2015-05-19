/*
  chronyd/chronyc - Programs for keeping computer clocks accurate.

 **********************************************************************
 * Copyright (C) Richard P. Curnow  1997-2003
 * Copyright (C) Miroslav Lichvar  2012-2014
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

  Module for managing keys used for authenticating NTP packets and commands

  */

#include "config.h"

#include "sysincl.h"

#include "array.h"
#include "keys.h"
#include "cmdparse.h"
#include "conf.h"
#include "memory.h"
#include "util.h"
#include "local.h"
#include "logging.h"


typedef struct {
  uint32_t id;
  char *val;
  int len;
  int hash_id;
  int auth_delay;
} Key;

static ARR_Instance keys;

static int command_key_valid;
static uint32_t command_key_id;
static int cache_valid;
static uint32_t cache_key_id;
static int cache_key_pos;

/* ================================================== */

static int
generate_key(uint32_t key_id)
{
#ifdef FEAT_SECHASH
  unsigned char key[20];
  const char *hashname = "SHA1";
#else
  unsigned char key[16];
  const char *hashname = "MD5";
#endif
  const char *key_file, *rand_dev = "/dev/urandom";
  FILE *f;
  struct stat st;
  int i;

  key_file = CNF_GetKeysFile();

  if (!key_file)
    return 0;

  f = fopen(rand_dev, "r");
  if (!f || fread(key, sizeof (key), 1, f) != 1) {
    if (f)
      fclose(f);
    LOG_FATAL(LOGF_Keys, "Could not read %s", rand_dev);
    return 0;
  }
  fclose(f);

  f = fopen(key_file, "a");
  if (!f) {
    LOG_FATAL(LOGF_Keys, "Could not open keyfile %s for writing", key_file);
    return 0;
  }

  /* Make sure the keyfile is not world-readable */
  if (stat(key_file, &st) || chmod(key_file, st.st_mode & 0770)) {
    fclose(f);
    LOG_FATAL(LOGF_Keys, "Could not change permissions of keyfile %s", key_file);
    return 0;
  }

  fprintf(f, "\n%"PRIu32" %s HEX:", key_id, hashname);
  for (i = 0; i < sizeof (key); i++)
    fprintf(f, "%02hhX", key[i]);
  fprintf(f, "\n");
  fclose(f);

  /* Erase the key from stack */
  memset(key, 0, sizeof (key));

  LOG(LOGS_INFO, LOGF_Keys, "Generated key %"PRIu32, key_id);

  return 1;
}

/* ================================================== */

static void
free_keys(void)
{
  unsigned int i;

  for (i = 0; i < ARR_GetSize(keys); i++)
    Free(((Key *)ARR_GetElement(keys, i))->val);

  ARR_SetSize(keys, 0);
  command_key_valid = 0;
  cache_valid = 0;
}

/* ================================================== */

void
KEY_Initialise(void)
{
  keys = ARR_CreateInstance(sizeof (Key));
  command_key_valid = 0;
  cache_valid = 0;
  KEY_Reload();

  if (CNF_GetGenerateCommandKey() && !KEY_KeyKnown(KEY_GetCommandKey())) {
    if (generate_key(KEY_GetCommandKey()))
      KEY_Reload();
  }
}

/* ================================================== */

void
KEY_Finalise(void)
{
  free_keys();
  ARR_DestroyInstance(keys);
}

/* ================================================== */

static Key *
get_key(unsigned int index)
{
  return ((Key *)ARR_GetElements(keys)) + index;
}

/* ================================================== */

static int
determine_hash_delay(uint32_t key_id)
{
  NTP_Packet pkt;
  struct timeval before, after;
  unsigned long usecs, min_usecs=0;
  int i;

  for (i = 0; i < 10; i++) {
    LCL_ReadRawTime(&before);
    KEY_GenerateAuth(key_id, (unsigned char *)&pkt, NTP_NORMAL_PACKET_LENGTH,
        (unsigned char *)&pkt.auth_data, sizeof (pkt.auth_data));
    LCL_ReadRawTime(&after);

    usecs = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);

    if (i == 0 || usecs < min_usecs) {
      min_usecs = usecs;
    }
  }

  /* Add on a bit extra to allow for copying, conversions etc */
  min_usecs += min_usecs >> 4;

  DEBUG_LOG(LOGF_Keys, "authentication delay for key %"PRIu32": %ld useconds", key_id, min_usecs);

  return min_usecs;
}

/* ================================================== */

/* Compare two keys */

static int
compare_keys_by_id(const void *a, const void *b)
{
  const Key *c = (const Key *) a;
  const Key *d = (const Key *) b;

  if (c->id < d->id) {
    return -1;
  } else if (c->id > d->id) {
    return +1;
  } else {
    return 0;
  }

}

/* ================================================== */

void
KEY_Reload(void)
{
  unsigned int i, line_number;
  FILE *in;
  uint32_t key_id;
  char line[2048], *keyval, *key_file;
  const char *hashname;
  Key key;

  free_keys();

  key_file = CNF_GetKeysFile();
  line_number = 0;

  if (!key_file)
    return;

  in = fopen(key_file, "r");
  if (!in) {
    LOG(LOGS_WARN, LOGF_Keys, "Could not open keyfile %s", key_file);
    return;
  }

  while (fgets(line, sizeof (line), in)) {
    line_number++;

    CPS_NormalizeLine(line);
    if (!*line)
      continue;

    if (!CPS_ParseKey(line, &key_id, &hashname, &keyval)) {
      LOG(LOGS_WARN, LOGF_Keys, "Could not parse key at line %d in file %s", line_number, key_file);
      continue;
    }

    key.hash_id = HSH_GetHashId(hashname);
    if (key.hash_id < 0) {
      LOG(LOGS_WARN, LOGF_Keys, "Unknown hash function in key %"PRIu32, key_id);
      continue;
    }

    key.len = UTI_DecodePasswordFromText(keyval);
    if (!key.len) {
      LOG(LOGS_WARN, LOGF_Keys, "Could not decode password in key %"PRIu32, key_id);
      continue;
    }

    key.id = key_id;
    key.val = MallocArray(char, key.len);
    memcpy(key.val, keyval, key.len);
    ARR_AppendElement(keys, &key);
  }

  fclose(in);

  /* Sort keys into order.  Note, if there's a duplicate, it is
     arbitrary which one we use later - the user should have been
     more careful! */
  qsort(ARR_GetElements(keys), ARR_GetSize(keys), sizeof (Key), compare_keys_by_id);

  /* Check for duplicates */
  for (i = 1; i < ARR_GetSize(keys); i++) {
    if (get_key(i - 1)->id == get_key(i)->id)
      LOG(LOGS_WARN, LOGF_Keys, "Detected duplicate key %"PRIu32, get_key(i - 1)->id);
  }

  /* Erase any passwords from stack */
  memset(line, 0, sizeof (line));

  for (i = 0; i < ARR_GetSize(keys); i++)
    get_key(i)->auth_delay = determine_hash_delay(get_key(i)->id);
}

/* ================================================== */

static int
lookup_key(uint32_t id)
{
  Key specimen, *where, *keys_ptr;
  int pos;

  keys_ptr = ARR_GetElements(keys);
  specimen.id = id;
  where = (Key *)bsearch((void *)&specimen, keys_ptr, ARR_GetSize(keys),
                         sizeof (Key), compare_keys_by_id);
  if (!where) {
    return -1;
  } else {
    pos = where - keys_ptr;
    return pos;
  }
}

/* ================================================== */

static Key *
get_key_by_id(uint32_t key_id)
{
  int position;

  if (cache_valid && key_id == cache_key_id)
    return get_key(cache_key_pos);

  position = lookup_key(key_id);

  if (position >= 0) {
    cache_valid = 1;
    cache_key_pos = position;
    cache_key_id = key_id;

    return get_key(position);
  }

  return NULL;
}

/* ================================================== */

uint32_t
KEY_GetCommandKey(void)
{
  if (!command_key_valid) {
    command_key_id = CNF_GetCommandKey();
  }

  return command_key_id;
}

/* ================================================== */

int
KEY_KeyKnown(uint32_t key_id)
{
  return get_key_by_id(key_id) != NULL;
}

/* ================================================== */

int
KEY_GetAuthDelay(uint32_t key_id)
{
  Key *key;

  key = get_key_by_id(key_id);

  if (!key)
    return 0;

  return key->auth_delay;
}

/* ================================================== */

int
KEY_GenerateAuth(uint32_t key_id, const unsigned char *data, int data_len,
    unsigned char *auth, int auth_len)
{
  Key *key;

  key = get_key_by_id(key_id);

  if (!key)
    return 0;

  return UTI_GenerateNTPAuth(key->hash_id, (unsigned char *)key->val,
                             key->len, data, data_len, auth, auth_len);
}

/* ================================================== */

int
KEY_CheckAuth(uint32_t key_id, const unsigned char *data, int data_len,
    const unsigned char *auth, int auth_len)
{
  Key *key;

  key = get_key_by_id(key_id);

  if (!key)
    return 0;

  return UTI_CheckNTPAuth(key->hash_id, (unsigned char *)key->val,
                          key->len, data, data_len, auth, auth_len);
}
