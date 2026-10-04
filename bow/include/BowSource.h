/*
 * BowSource.h
 *
 * Copyright (c) 1995-1997,1999, John Kilburg <john@cs.unlv.edu>
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
#ifndef __BOWSOURCE_H__
#define __BOWSOURCE_H__ 1

#include "common.h"
#include "mime.h"
#include "url.h"

typedef struct BowSourceP *BowSource;
typedef struct BowSinkP *BowSink;
typedef struct BowRequestP BowRequest;

typedef struct
{
  void *closure;
  int (*init) _ArgProto((BowSink, void *));  /* returns 0 for success */
  void (*add) _ArgProto((void *));
  void (*end) _ArgProto((void *));
  void (*message) _ArgProto((void *, char *));
} BowSinkHooks;

typedef struct
{
  char *name;
  void *class_closure;
  void *(*init) _ArgProto((BowSource, BowRequest *, void *));
  void (*stop) _ArgProto((void *));
  void (*destroy) _ArgProto((void *));
  void (*class_destroy) _ArgProto((void *));
  void (*getdata) _ArgProto((void *, byte **, size_t *, MIMEHeader *));
  char *(*resolve_url) _ArgProto((MemPool, char *, char *));
} BowSourceHooks;

struct BowRequestP
{
  MemPool mp;                /* */
  char *url;                 /* Absolute URL not-parsed */
  URLParts *up;              /* Absolute URL parsed */
  URLParts *pup;             /* Absolute proxy URL parsed */
  void *input_data;          /* input data */
  size_t input_len;          /* input data len */
  char *input_type;          /* input data MIME content-type */
  char *input_method;        /* input method GET/POST */
  bool reload;               /* don't use cache? */
  GList contents;            /* acceptable contents, NULL = '*' */
  BowSourceHooks hooks;  /* */

  char *scheme;              /* convienence */
  char *parent_url;          /* parent/base url saved from RequestCreate */
};

/*
 * Source
 *
 *  Functions called by the source implementation.
 */
void SourceInit _ArgProto((BowSource, bool));
void SourceAdd _ArgProto((BowSource));
void SourceEnd _ArgProto((BowSource));
void SourceStop _ArgProto((BowSource, char *));
void SourceSendMessage _ArgProto((BowSource, char *));
BowResources SourceToResources _ArgProto((BowSource));

int SourceAddHooks _ArgProto((BowResources cres,
			      BowSourceHooks *shooks));

/*
 * Sink
 *
 * Functions called by the sink creator.
 */
BowSink SinkCreate _ArgProto((BowResources, BowRequest *));
void SinkSetHooks _ArgProto((BowSink, BowSinkHooks *, void *));
void SinkDestroy _ArgProto((BowSink));
void SinkCancel _ArgProto((BowSink));
void SinkGetData _ArgProto((BowSink, byte **, size_t *, MIMEHeader *));
char *SinkGetInfo _ArgProto((BowSink, char *));
BowResources SinkToResources _ArgProto((BowSink));
bool SinkWasReloaded _ArgProto((BowSink));

/*
 * request.c
 */
BowRequest *RequestCreate _ArgProto((BowResources, char *, char *));
int RequestAddRegexContent _ArgProto((BowResources,
				      BowRequest *, char *));
void RequestAddContent _ArgProto((BowRequest *, char *));
void RequestDestroy _ArgProto((BowRequest *));
void RequestReload _ArgProto((BowRequest *, bool));
bool RequestCompareURL _ArgProto((BowRequest *, BowRequest *));
bool RequestCompareAccept _ArgProto((BowRequest *, BowRequest *));
bool RequestMatchContent _ArgProto((MemPool, char *, char *));
bool RequestMatchContent2 _ArgProto((BowRequest *, char *));

char *BowExt2Content _ArgProto((BowResources, char *));

#endif
