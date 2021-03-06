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

  Header for key management module
  */

#ifndef GOT_KEYS_H
#define GOT_KEYS_H

extern void KEY_Initialise(void);
extern void KEY_Finalise(void);

extern void KEY_Reload(void);

extern unsigned long KEY_GetCommandKey(void);

extern int KEY_GetKey(unsigned long key_id, char **key, int *len);
extern int KEY_KeyKnown(unsigned long key_id);
extern int KEY_GetAuthDelay(unsigned long key_id);

extern int KEY_GenerateAuth(unsigned long key_id, const unsigned char *data,
    int data_len, unsigned char *auth, int auth_len);
extern int KEY_CheckAuth(unsigned long key_id, const unsigned char *data,
    int data_len, const unsigned char *auth, int auth_len);

#endif /* GOT_KEYS_H */
