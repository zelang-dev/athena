/*
 * BowRender.h
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
#ifndef __BOWRENDER_H__
#define __BOWRENDER_H__ 1

#include "common.h"
#include "url.h"

#include "BowGUI.h"
#include "BowSource.h"

/*
 * A whole bunch of types with which to cause trouble and confuse.
 */
typedef struct BowRenderP *BowRender;

typedef struct
{
  char *content;
  void *class_context;
  void *(*init) _ArgProto((BowRender, void *, void *));
  void (*add) _ArgProto((void *));
  void (*end) _ArgProto((void *));
  void (*destroy) _ArgProto((void *));
  void (*cancel) _ArgProto((void *));
  void *(*getstate) _ArgProto((void *));
  void (*class_destroy) _ArgProto((void *));
  byte *(*query) _ArgProto((void *, char *));
  int (*search) _ArgProto((void *, char *, int));
  bool (*select) _ArgProto((void *, int, int, char *));
  bool (*motion) _ArgProto((void *, int, int));
  bool (*expose) _ArgProto((void *, int, int, unsigned int, unsigned int));
} BowRenderHooks;

/*
 * render.c
 */

typedef void (*BowRenderActionProc) _ArgProto((void *, BowRender,
						   BowRequest *,
						   char *));

/* Called by the render user/caller */
BowRender RenderCreate _ArgProto((BowContext,
				      BowGUI, BowSink,
				      BowRenderHooks *,
				      BowRenderHooks *,
				      void *, void *,
				      BowRenderActionProc,
				      void *));
void RenderAdd _ArgProto((BowRender));
void RenderEnd _ArgProto((BowRender));
void RenderDestroy _ArgProto((BowRender));
void RenderCancel _ArgProto((BowRender));
char *RenderQuery _ArgProto((BowRender, char *));

/*
 * Returns -1 for not found
 * Returns -2 for end of document
 * Returns 0 for found
 *
 * third argument 0 for continue search
 * third argument 1 for start search from beginning
 */
int RenderSearch _ArgProto((BowRender, char *, int));
void RenderSelect _ArgProto((BowRender, int, int, char *));
void RenderMotion _ArgProto((BowRender, int, int));
void RenderExpose _ArgProto((BowRender, int, int,
			     unsigned int, unsigned int));

void *RenderGetState _ArgProto((BowRender));

BowGUI RenderToGUI _ArgProto((BowRender));
BowSink RenderToSink _ArgProto((BowRender));
BowContext RenderToContext _ArgProto((BowRender));

void RenderSendMessage _ArgProto((BowRender, char *));

void RenderAction _ArgProto((BowRender, BowRequest *, char *));

int RenderAddHooks _ArgProto((BowResources, BowRenderHooks *));
BowRenderHooks *RenderGetHooks _ArgProto((BowResources, char *));

BowResources RenderToResources _ArgProto((BowRender));

#endif
