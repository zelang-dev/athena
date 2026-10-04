/*
 * Bow.h
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
#ifndef __BOW_H__
#define __BOW_H__ 1

#include "common.h"
#include "url.h"
#include <tls.h>
#include <athena.h>

typedef struct BowResourcesP   *BowResources;
typedef struct BowContextP     *BowContext;

typedef struct BowTaskP      *BowTask;
typedef void (*BowTaskProc) _ArgProto((void *));

#define TaskSchedule(a, b, c)    TaskScheduleX(a, b, c, __LINE__, __FILE__);

BowTask TaskScheduleX _ArgProto((BowResources,
				     BowTaskProc, void *, int, char *));
void TaskRemove _ArgProto((BowResources, BowTask));

typedef struct BowTimeOutP *BowTimeOut;
typedef void (*BowTimeOutProc) _ArgProto((BowTimeOut, void *));

BowTimeOut TimeOutCreate _ArgProto((BowResources, unsigned int,
					BowTimeOutProc, void *));
void TimeOutDestroy _ArgProto((BowTimeOut));

char *ResourceGetString _ArgProto((BowResources, char *));
int ResourceAddFile _ArgProto((BowResources, char *));
int ResourceAddString _ArgProto((BowResources, char *));
char *ResourceGetBool _ArgProto((BowResources, char *, bool *));
char *ResourceGetInt _ArgProto((BowResources, char *, int *));
char *ResourceGetUInt _ArgProto((BowResources, char *, unsigned int *));
char *ResourceGetFilename _ArgProto((BowResources, MemPool, char *));

void CMethodVoidDoom();
void *CMethodVoidPtrDoom();
int CMethodIntDoom();
char CMethodCharDoom();
char *CMethodCharPtrDoom();
byte *CMethodBytePtrDoom();
bool CMethodBoolDoom();

#define CMethod(x) ((x) != NULL ? (x):CMethodVoidDoom)
#define CMethodChar(x) ((x) != NULL ? (x):CMethodCharDoom)
#define CMethodCharPtr(x) ((x) != NULL ? (x):CMethodCharPtrDoom)
#define CMethodBytePtr(x) ((x) != NULL ? (x):CMethodBytePtrDoom)
#define CMethodBool(x) ((x) != NULL ? (x):CMethodBoolDoom)
#define CMethodInt(x) ((x) != NULL ? (x):CMethodIntDoom)
#define CMethodVoid(x) ((x) != NULL ? (x):CMethodVoidDoom)
#define CMethodVoidPtr(x) ((x) != NULL ? (x):CMethodVoidPtrDoom)

#endif
