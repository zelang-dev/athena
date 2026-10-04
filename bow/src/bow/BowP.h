/*
 * BowP.h
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
#ifndef __BOWP_H__
#define __BOWP_H__

#include "common.h"

#include "Bow.h"
#include "BowSource.h"
#include "BowGUI.h"
#include "BowRender.h"
#include "BowAuth.h"
#include "BowStack.h"

typedef struct BowCacheP *BowCache;
typedef struct BookmarkContextP *BookmarkContext;
typedef struct BowSchedulerP *BowScheduler;

struct BowContextP
{
  MemPool mp;

  Widget toplevel;
  Widget file;
  Widget www;
  Widget open;
  Widget back;
  Widget reload;
  Widget cancel;
  Widget quit;
  Widget home;
  Widget addmark;
  Widget viewmark;
  Widget message;
  Widget dup;
  Widget source;
  Widget save;
  Widget openpop;
  Widget savepop;
  Widget findpop;
  Widget bookpop;
  Widget url;
  Widget help;
  Widget bookmark;
  Widget find;

  BowStack       tstack;  /* top stack */
  BowStack       cstack;  /* current stack */
  MemPool            tmp;

  BowResources cres;

  char *button1Box;           /* list of widgets in the first button box */
  char *button2Box;           /* list of widgets in the second button box */
  ats_t *athena;				  /* `athena` handle */
};

/*
 * Bow runtime context
 */
struct BowResourcesP
{
  int              maxiocnt;          /* maximum IOs */
  bool             printLoadMessages;
  bool             printTaskInfo;
  MemPool          mp;                /* memory pool */
  XrmDatabase      db;                /* message database */

  BowCache     cc;

  /* Lists of stuff we need to know about */
  GList            sourcehooks;       /* list of registered sources */
  GList            renderhooks;       /* list of registered renderers */
  GList            mimes;             /* list of mimes */
  GList            renderers;         /* list of active renderers */
  GList            sources;           /* list of active sources */
  GList            timeouts;          /* list of timeouts */
  GList            oldtimeouts;       /* list of old timeouts */
  GList            heads;             /* list of heads */
  GList            stacks;            /* list of stacks */

  /* */
  void             (*authui_callback)();
  void             *authui_closure;

  XtAppContext     appcon;            /* X application context */
  size_t           id;                /* next unique ID */

  Display          *dpy;

  BowScheduler cs;                /* task scheduler context */
  BowContext   bmcontext;         /* head associated with bookmarks */
  BowRenderHooks  *plainhooks;    /* plain text renderer */
  BookmarkContext  bc;                /* bookmark routines context */
  FILE             *logfp;            /* log file */
  int              refcount;          /* reference count */
  ats_t 		   *ats;			  /* `athena` handle */
};

/*
 *
 * Prototypes
 *
 */

/*
 * cache.c
 */
BowCache CacheCreate _ArgProto((BowResources));
void CacheDestroy _ArgProto((BowCache));
bool CacheIsDiskCached _ArgProto((BowCache, char *));
int CacheWrite _ArgProto((BowCache, BowSource));
BowSourceHooks *CacheGetHooks _ArgProto((BowCache));

/*
 * data.c
 */
void SourceGetData _ArgProto((BowSource,
			      byte **, size_t *, MIMEHeader *));

/*
 * gui.c
 */
BowGUI GUICreateToplevel _ArgProto((BowContext, Widget,
					GUISizeCallback, void *));
void GUIReset _ArgProto((BowGUI));
void GUIAddRender _ArgProto((BowGUI, BowRender));

/*
 * bookmark.c
 */
BookmarkContext BookmarkCreateContext _ArgProto((BowResources));
void BookmarkDestroyContext _ArgProto((BookmarkContext));
void BookmarkAdd _ArgProto((BookmarkContext, char *, char *));
void BookmarkShow _ArgProto((BookmarkContext));

/*
 * head.c
 */
void HeadCreate _ArgProto((BowResources, BowRequest *,
			   BowRequest *));
void HeadDestroy _ArgProto((BowContext));
void HeadPrintMessage _ArgProto((BowContext, char *));
void HeadPrintURL _ArgProto((BowContext, char *));

Widget CreateDialog _ArgProto((Widget, char *,
			       void (*)(), void (*)(), void (*)(), XtPointer));
Widget GetDialogWidget _ArgProto((Widget));

/*
 * main.c
 */
void BowAddReference _ArgProto((BowResources));
void BowRemoveReference _ArgProto((BowResources));

/*
 * callback.c
 */
bool MessageCallback _ArgProto((void *, char *));
bool ActionCallback _ArgProto((void *, BowRequest *, char *));
bool RedrawCallback _ArgProto((void *));
bool ResizeCallback _ArgProto((void *));

/*
 * builtin.c
 */
void InitBowBuiltins _ArgProto((BowResources));

/*
 * view.c
 */
void ViewOpen _ArgProto((BowResources, BowRequest *));

/*
 * download.c
 */
void DownloadOpen _ArgProto((BowResources, BowRequest *));

/*
 * stack.c
 */
BowStack StackCreateToplevel _ArgProto((BowContext, Widget));
BowGUI StackToGUI _ArgProto((BowStack));
BowRender StackToRender _ArgProto((BowStack));
void StackSetRender _ArgProto((BowStack, BowRenderHooks *));

/*
 * resource.c
 */
char *ResourceGetStringP _ArgProto((BowResources, char *));

/*
 * cmisc.c
 */
BowSourceHooks *SourceGetHooks _ArgProto((BowResources, char *));

/*
 * task.c
 */
BowScheduler SchedulerCreate _ArgProto((void));

#endif
