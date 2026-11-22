/*
 * builtin.c
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
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

#include <stdio.h>

#include <X11/Intrinsic.h>


#include "Bow.h"

extern void InitModule_HTML(BowResources);
extern void InitModule_Plain(BowResources);
extern void InitModule_HTTP(BowResources);
extern void InitModule_FTP(BowResources);
extern void InitModule_File(BowResources);
extern void InitModule_Image(BowResources);
extern void InitModule_Mailto(BowResources);
extern void InitModule_Data(BowResources);

void InitBowBuiltins(BowResources cres) {
  InitModule_Plain(cres);
  InitModule_Image(cres);
  InitModule_HTML(cres);
  InitModule_HTTP(cres);
  InitModule_FTP(cres);
  InitModule_File(cres);
  InitModule_Mailto(cres);
  InitModule_Data(cres);
}
