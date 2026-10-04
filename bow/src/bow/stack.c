/*
 * stack.c
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
#include <stdlib.h>
#include <string.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>


#include "BowP.h"

typedef enum
{
  StackNewAction,
  StackBackAction,
  StackReloadAction,
  StackReplaceAction,
  StackRedrawAction,
  StackHomeAction
} StackOpAction;

typedef struct
{
  char *url;
  void *state; /*
		* this is the state of the renderer (mostly meant for
		* scrollbar position) to be used when the document is
		* redisplayed.
		*/
} URLInfo;

struct BowStackP
{
  MemPool            mp, tmp;
  BowGUI         wd;
  BowContext     wc;
  BowStack       parent;

  StackOpAction      action;
  BowRender      wn;
  BowSink        wp;
  BowSink        pending;

  BowRenderActionProc rap;
  void               *rapc;

  BowRenderHooks *rh;

  bool               toplevel;

  /* list for URL stack */
  GList              urls;
  GList              ourls;
};

static void StackGUIResize _ArgProto((BowGUI, void *,
				      unsigned int, unsigned int));
static bool StackPop _ArgProto((BowStack));
static void StackOp _ArgProto((BowStack, BowRequest *,
			       StackOpAction));
int StackSinkInit _ArgProto((BowSink, void *));
static void StackSinkAdd _ArgProto((void *));
static void StackSinkEnd _ArgProto((void *));
static void StackSinkMessage _ArgProto((void *, char *));
static void StackRenderAction _ArgProto((void *, BowRender,
					 BowRequest *, char *));

/*
 * StackPop
 */
static bool
StackPop(cs)
BowStack cs;
{
  void *muck;

  if ((muck = GListPop(cs->urls)) != NULL)
  {
    GListAddHead(cs->ourls, muck);
    return(true);
  }

  return(false);
}

/*
 * StackMessage
 */
static void
StackSinkMessage(closure, message)
void *closure;
char *message;
{
  BowStack cs = (BowStack)closure;

  HeadPrintMessage(cs->wc, message);

  return;
}

/*
 * StackEnd
 */
static void
StackSinkEnd(closure)
void *closure;
{
  BowStack cs = (BowStack)closure;

  RenderEnd(cs->wn);

  return;
}

/*
 * StackSinkAdd
 */
static void
StackSinkAdd(closure)
void *closure;
{
  BowStack cs = (BowStack)closure;

  RenderAdd(cs->wn);
  return;
}

/*
 * StackOpen
 */
void
StackOpen(cs, wr)
BowStack cs;
BowRequest *wr;
{
  StackOp(cs, wr, StackNewAction);
  return;
}

/*
 * StackHome
 */
void
StackHome(cs)
BowStack cs;
{
  URLInfo *u;

  if (GListGetTail(cs->urls) == GListGetHead(cs->urls)) return;
  if ((u = (URLInfo *)GListGetTail(cs->urls)) == NULL) return;
  StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackHomeAction);

  return;
}

/*
 * StackBack
 */
void
StackBack(cs)
BowStack cs;
{
  URLInfo *u;

  if (GListGetHead(cs->urls) == NULL) return;
  if ((u = (URLInfo *)GListGetNext(cs->urls)) == NULL) return;
  StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackBackAction);

  return;
}

/*
 * StackReload
 */
void
StackReload(cs)
BowStack cs;
{
  URLInfo *u;
  BowRequest *wr;

  if ((u = GListGetHead(cs->urls)) == NULL) return;
  wr = RequestCreate(cs->wc->cres, u->url, NULL);
  RequestReload(wr, true);
  StackOp(cs, wr, StackReloadAction);

  return;
}

/*
 * StackRedraw
 */
void
StackRedraw(cs)
BowStack cs;
{
  URLInfo *u;

  if ((u = GListGetHead(cs->urls)) == NULL) return;
  StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackReplaceAction);

  return;
}

/*
 * StackCancel
 */
void
StackCancel(cs)
BowStack cs;
{
  if (cs->wp != NULL) SinkCancel(cs->wp);
  if (cs->pending != NULL) SinkCancel(cs->pending);
  if (cs->wn != NULL) RenderCancel(cs->wn);

  return;
}

/*
 * StackAction
 */
void
StackAction(cs, wr, action)
BowStack cs;
BowRequest *wr;
char *action;
{
  MemPool mp;
  char *rstr;
  char *rname;
  size_t len;
  bool newhead;

  /*
   * Should probably give an error message here.
   */
  if (wr == NULL) return;

  if (strcmp("open", action) == 0)
  {
    mp = MPCreate();
    len = strlen(wr->scheme) + strlen(".newhead");
    rname = (char *)MPGet(mp, len + 1);
    strncpy(rname, wr->scheme, len);
    strncat(rname, ".newhead", len);
    rname[len] = '\0';
    if ((rstr = ResourceGetBool(cs->wc->cres, rname, &newhead)) == NULL ||
	!newhead)
    {
      StackOpen(cs, wr);
    }
    else
    {
      HeadCreate(cs->wc->cres, wr, NULL);
    }
    MPDestroy(mp);
  }
  else if (strcmp("download", action) == 0) DownloadOpen(cs->wc->cres, wr);
  else if (strcmp("external", action) == 0) ViewOpen(cs->wc->cres, wr);
  else fprintf (stderr, "Action cannot be taken.  Bug.\n");

  return;
}

/*
 * StackRenderAction
 */
static void
StackRenderAction(closure, wn, wr, action)
void *closure;
BowRender wn;
BowRequest *wr;
char *action;
{
  StackAction((BowStack)closure, wr, action);

  return;
}

/*
 * StackSinkInit
 */
int
StackSinkInit(wp, closure)
BowSink wp;
void *closure;
{
  BowStack cs = (BowStack)closure;
  URLInfo *u;
  void *newstate, *curstate;
  char *url;
  char *ctype;
  BowRenderHooks *rh;
  BowRequest *wr;

  myassert(wp == cs->pending, "Data init is not the same as data pending.");

  url = SinkGetInfo(wp, "x-url");
  ctype = SinkGetInfo(wp, "content-type");

  if (cs->rh != NULL) rh = cs->rh;
  else
  {
    if ((rh = RenderGetHooks(cs->wc->cres, ctype)) == NULL)
    {
      if ((wr = RequestCreate(cs->wc->cres, url, NULL)) == NULL)
      {
	HeadPrintMessage(cs->wc, "Invalid URL.");
      }
      else ViewOpen(cs->wc->cres, wr);
      SinkDestroy(wp);
      cs->pending = NULL;
      return(-1);
    }
  }

  if (cs->wn != NULL)
  {
    curstate = RenderGetState(cs->wn);
    GUIReset(cs->wd);
    RenderDestroy(cs->wn);
    cs->wn = NULL;
  }
  else curstate = NULL;

  if (cs->wp != NULL) SinkDestroy(cs->wp);
  cs->wp = cs->pending;
  cs->pending = NULL;

  if (cs->action == StackBackAction)
  {
    StackPop(cs);
    u = (URLInfo *)GListGetHead(cs->urls);
    newstate = u->state;
    StackPop(cs);
  }
  else if (cs->action == StackReloadAction ||
	   cs->action == StackReplaceAction)
  {
    u = (URLInfo *)GListGetHead(cs->urls);
    newstate = u->state;
    StackPop(cs);
  }
  else if (cs->action == StackHomeAction)
  {
    u = (URLInfo *)GListGetTail(cs->urls);
    newstate = u->state;
    while (StackPop(cs))
	;
    MPDestroy(cs->tmp);
    cs->tmp = MPCreate();
  }
  else
  {
    if ((u = (URLInfo *)GListGetHead(cs->urls)) != NULL)
    {
      u->state = curstate;
    }
    newstate = NULL;
  }

  if ((u = (URLInfo *)GListPop(cs->ourls)) == NULL)
  {
    u = (URLInfo *)MPCGet(cs->mp, sizeof(URLInfo));
  }
  u->url = MPStrDup(cs->tmp, url);
  u->state = newstate;

  GListAddHead(cs->urls, u);

  if ((cs->wn = RenderCreate(cs->wc, cs->wd,
			     cs->wp, rh, NULL, NULL, u->state,
			     cs->rap, cs->rapc)) == NULL)
  {
    DownloadOpen(cs->wc->cres, wr);
    SinkDestroy(wp);
    return(-1);
  }

  if (cs->toplevel) HeadPrintURL(cs->wc, url);

  return(0);
}

/*
 * StackGetCurrentURL
 */
char *
StackGetCurrentURL(cs)
BowStack cs;
{
  URLInfo *u;

  if ((u = (URLInfo *)GListGetHead(cs->urls)) == NULL) return(NULL);
  return(u->url);
}

/*
 * StackOp
 */
static void
StackOp(cs, wr, action)
BowStack cs;
BowRequest *wr;
StackOpAction action;
{
  BowSinkHooks hooks;

  if (wr == NULL)
  {
    HeadPrintMessage(cs->wc, "Invalid URL");
    return;
  }

  StackCancel(cs);
  if (cs->pending != NULL)
  {
    SinkDestroy(cs->pending);
    cs->pending = NULL;
  }

  cs->action = action;

  memset(&hooks, 0, sizeof(hooks));
  hooks.init = StackSinkInit;
  hooks.add = StackSinkAdd;
  hooks.end = StackSinkEnd;
  hooks.message = StackSinkMessage;

  /* First, pick all of the contents we can deal with directly */
  RequestAddRegexContent(cs->wc->cres, wr, "*/*");
  /* Add whatever we can get onto the end */
  RequestAddContent(wr, "*/*");

  if (wr->url != NULL && cs->wc->cres->logfp != NULL)
  {
    fprintf (cs->wc->cres->logfp, "%s\n", wr->url);
  }

  cs->pending = SinkCreate(cs->wc->cres, wr);
  if (cs->pending != NULL) SinkSetHooks(cs->pending, &hooks, cs);

  return;
}

/*
 * StackGUIResize
 */
static void
StackGUIResize(wd, closure, width, height)
BowGUI wd;
void *closure;
unsigned int width, height;
{
  StackRedraw((BowStack)closure);
  return;
}

/*
 * StackCreateToplevel
 */
BowStack
StackCreateToplevel(wc, widget)
BowContext wc;
Widget widget;
{
  BowStack cs;
  MemPool mp;

  mp = MPCreate();
  cs = (BowStack)MPCGet(mp, sizeof(struct BowStackP));
  cs->wc = wc;
  cs->mp = mp;
  cs->tmp = MPCreate();
  cs->urls = GListCreateX(mp);
  cs->ourls = GListCreateX(mp);
  cs->rap = StackRenderAction;
  cs->rapc = cs;

  GListAddHead(wc->cres->stacks, cs);

  cs->wd = GUICreateToplevel(wc, widget, StackGUIResize, cs);

  cs->toplevel = true;

  return(cs);
}

/*
 * StackCreate
 */
BowStack
StackCreate(parent, x, y, width, height, rap, rapc)
BowStack parent;
int x, y;
unsigned int width, height;
BowRenderActionProc rap;
void *rapc;
{
  BowStack cs;
  MemPool mp;

  mp = MPCreate();
  cs = (BowStack)MPCGet(mp, sizeof(struct BowStackP));
  cs->parent = parent;
  cs->wc = parent->wc;
  cs->mp = mp;
  cs->tmp = MPCreate();
  cs->urls = GListCreateX(mp);
  cs->ourls = GListCreateX(mp);
  if (cs->rap == NULL || cs->rapc == NULL)
  {
    cs->rap = StackRenderAction;
    cs->rapc = cs;
  }

  GListAddHead(parent->wc->cres->stacks, cs);

  cs->wd = GUICreate(parent->wc, parent->wd, StackGUIResize, cs);
  GUISetInitialDimensions(cs->wd, width, height);
  GUIMap(cs->wd, x, y);

  return(cs);
}

/*
 * StackToRender
 */
BowRender
StackToRender(cs)
BowStack cs;
{
  return(cs->wn);
}

/*
 * StackToGUI
 */
BowGUI
StackToGUI(cs)
BowStack cs;
{
  return(cs->wd);
}

/*
 * StackSetRender
 */
void
StackSetRender(cs, rh)
BowStack cs;
BowRenderHooks *rh;
{
  cs->rh = rh;
  return;
}

/*
 * StackDestroy
 */
void
StackDestroy(cs)
BowStack cs;
{
  if (cs->pending != NULL) SinkDestroy(cs->pending);
  if (cs->wp != NULL) SinkDestroy(cs->wp);
  if (cs->wn != NULL) RenderDestroy(cs->wn);
  if (cs->wd != NULL) GUIDestroy(cs->wd);
  if (cs->tmp != NULL) MPDestroy(cs->tmp);
  GListRemoveItem(cs->wc->cres->stacks, cs);
  MPDestroy(cs->mp);
  return;
}

/*
 * StackFromGUI
 */
BowStack
StackFromGUI(wc, wd)
BowContext wc;
BowGUI wd;
{
  BowStack cs;

  for (cs = (BowStack)GListGetHead(wc->cres->stacks); cs != NULL;
       cs = (BowStack)GListGetNext(wc->cres->stacks))
  {
    if (cs->wd == wd) return(cs);
  }

  return(NULL);
}

/*
 * StackGetParent
 */
BowStack
StackGetParent(cs)
BowStack cs;
{
  return(cs->parent);
}
