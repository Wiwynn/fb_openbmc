/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LIGHTNING_COMMON_H__
#define __LIGHTNING_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MAX_NUM_FRUS 3
enum {
  FRU_ALL   = 0,
  FRU_PEB = 1,
  FRU_PDPB = 2,
  FRU_FCB = 3,
};

int lightning_common_fru_name(uint8_t fru, char *str);
int lightning_common_fru_id(char *str, uint8_t *fru);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __LIGHTNING_COMMON_H__ */
