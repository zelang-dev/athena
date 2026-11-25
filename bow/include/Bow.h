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

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include "url.h"
#include <tls.h>
#include <athena.h>

#if defined(USE_QUICKJS_NG) || defined(QUICKJS_NG)
#	include "quickjs-ng.h"
#	include "quickjs-libc-ng.h"
#	define JS_NAME	"QuickJS-NG"
#else
#	include "quickjs.h"
#	include "quickjs-libc.h"
#	define JS_NAME	"QuickJS"
#endif

#include "css.h"
#include "html.h"
#include "bow_engine.h"

typedef struct BowResourcesP *BowResources;
typedef struct BowContextP     *BowContext;

typedef struct BowTaskP      *BowTask;
typedef void (*BowTaskProc)(void *);

#define TaskSchedule(a, b, c)	TaskScheduleX(a, b, c, __LINE__, __FILE__);

BowTask TaskScheduleX(BowResources, BowTaskProc, void *, int, char *);
void TaskRemove(BowResources, BowTask);

typedef struct BowTimeOutP *BowTimeOut;
typedef void (*BowTimeOutProc)(BowTimeOut, void *);

BowTimeOut TimeOutCreate(BowResources, unsigned int,
					BowTimeOutProc, void *);
void TimeOutDestroy(BowTimeOut);

char *ResourceGetString(BowResources, char *);
int ResourceAddFile(BowResources, char *);
int ResourceAddString(BowResources, char *);
char *ResourceGetBool(BowResources, char *, bool *);
char *ResourceGetInt(BowResources, char *, int *);
char *ResourceGetUInt(BowResources, char *, unsigned int *);
char *ResourceGetFilename(BowResources, MemPool, char *);

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
