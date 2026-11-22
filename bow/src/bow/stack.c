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
#include <string.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "common.h"
#include "javascriptP.h"

typedef enum {
	StackNewAction,
	StackBackAction,
	StackReloadAction,
	StackReplaceAction,
	StackRedrawAction,
	StackHomeAction
} StackOpAction;

typedef struct {
	char *url;
	void *state; /*
		  * this is the state of the renderer (mostly meant for
		  * scrollbar position) to be used when the document is
		  * redisplayed.
		  */
} URLInfo;

struct BowStackP {
	MemPool            mp, tmp;
	BowGUI         wd;
	BowContext     wc;
	BowStack       parent;

	StackOpAction      action;
	BowRender      wn;
	BowSink        wp;
	BowSink        pending;

	BowRenderActionProc rap;
	void *rapc;

	BowRenderHooks *rh;

	bool               toplevel;

	/* list for URL stack */
	GList              urls;
	GList              ourls;
};

static void StackGUIResize(BowGUI, void *, unsigned int, unsigned int);
static bool StackPop(BowStack);
static void StackOp(BowStack, BowRequest *,	StackOpAction);
int StackSinkInit(BowSink, void *);
static void StackSinkAdd(void *);
static void StackSinkEnd(void *);
static void StackSinkMessage(void *, char *);
static void StackRenderAction(void *, BowRender, BowRequest *, char *);

/* Helper: decode percent-encoded characters in javascript: URLs */
static char *decode_js_uri(MemPool mp, const char *src) {
	size_t len = strlen(src);
	char *out = (char *)MPCGet(mp, len + 1);
	size_t i = 0, j = 0;

	while (i < len) {
		if (src[i] == '%' && i + 2 < len &&
			isxdigit((unsigned char)src[i + 1]) &&
			isxdigit((unsigned char)src[i + 2])) {
			char hex[3];
			hex[0] = src[i + 1];
			hex[1] = src[i + 2];
			hex[2] = '\0';
			out[j++] = (char)strtol(hex, NULL, 16);
			i += 3;
		} else {
			out[j++] = src[i++];
		}
	}
	out[j] = '\0';
	return out;
}

/*
 * StackPop
 */
static bool StackPop(BowStack cs) {
	void *muck;

	if ((muck = GListPop(cs->urls)) != NULL) {
		GListAddHead(cs->ourls, muck);
		return(true);
	}

	return(false);
}

/*
 * StackMessage
 */
static void StackSinkMessage(void *closure, char *message) {
	BowStack cs = (BowStack)closure;

	HeadPrintMessage(cs->wc, message);
}

/*
 * StackEnd
 */
static void StackSinkEnd(void *closure) {
	BowStack cs = (BowStack)closure;

	RenderEnd(cs->wn);
}

/*
 * StackSinkAdd
 */
static void StackSinkAdd(void *closure) {
	BowStack cs = (BowStack)closure;

	RenderAdd(cs->wn);
}

/*
 * StackOpen
 */
void StackOpen(BowStack cs, BowRequest *wr) {
	StackOp(cs, wr, StackNewAction);
}

/*
 * StackHome
 */
void StackHome(BowStack cs) {
	URLInfo *u;

	if (GListGetTail(cs->urls) == GListGetHead(cs->urls)) return;
	if ((u = (URLInfo *)GListGetTail(cs->urls)) == NULL) return;
	StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackHomeAction);
}

/*
 * StackBack
 */
void StackBack(BowStack cs) {
	URLInfo *u;

	if (GListGetHead(cs->urls) == NULL) return;
	if ((u = (URLInfo *)GListGetNext(cs->urls)) == NULL) return;
	StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackBackAction);
}

/*
 * StackReload
 */
void StackReload(BowStack cs) {
	URLInfo *u;
	BowRequest *wr;

	if ((u = GListGetHead(cs->urls)) == NULL) return;
	wr = RequestCreate(cs->wc->cres, u->url, NULL);
	RequestReload(wr, true);
	StackOp(cs, wr, StackReloadAction);
}

/*
 * StackRedraw
 */
void StackRedraw(BowStack cs) {
	URLInfo *u;

	if ((u = GListGetHead(cs->urls)) == NULL) return;

	/* Preserve live QuickJS DOM state across window resizes */
	if (cs->wc != NULL && cs->wc->cres != NULL &&
		cs->wc->cres->active_html != NULL &&
		cs->wc->cres->qjs != NULL &&
		cs->wc->cres->qjs->dom_root != NULL) {
		HTMLRepaintFromDOM((HTMLInfo)cs->wc->cres->active_html);
		return;
	}

	StackOp(cs, RequestCreate(cs->wc->cres, u->url, NULL), StackReplaceAction);
}

/*
 * StackCancel
 */
void StackCancel(BowStack cs) {
	if (cs->wp != NULL) SinkCancel(cs->wp);
	if (cs->pending != NULL) SinkCancel(cs->pending);
	if (cs->wn != NULL) RenderCancel(cs->wn);
}

static void BowDispatchDOMClick(BowResources cres, HtmlDom_t *node) {
	const char *code;
	char *script;

	if (!cres || !node || !qjs_is_running(cres))
		return;

	/* 1. Run inline onclick="..." attribute if present */
	code = dom_get_attr(node, "onclick");
	if (code != NULL && *code != '\0') {
		/* Execute with `this` bound or in global scope */
		bow_reEval_js(cres, code);
	}

	/* 2. Run JS property/listener if attached to node->js_onclick */
	if (JS_IsFunction(cres->qjs->ctx, node->js_onclick)) {
		JSValue this_obj = JS_IsUndefined(node->js_object)
			? JS_GetGlobalObject(cres->qjs->ctx)
			: JS_DupValue(cres->qjs->ctx, node->js_object);
		JSValue ret = JS_Call(cres->qjs->ctx, node->js_onclick, this_obj, 0, NULL);
		JS_FreeValue(cres->qjs->ctx, ret);
		JS_FreeValue(cres->qjs->ctx, this_obj);

		if (cres->active_html != NULL)
			HTMLRepaintFromDOM((HTMLInfo)cres->active_html);
	}
}

static void BowDispatchDOMLoad(BowResources cres) {
	const char *onload_script =
		"(function() {"
		"  if (typeof window.onload === 'function') window.onload();"
		"  if (document.body) {"
		"    var code = document.body.getAttribute('onload');"
		"    if (code) (new Function(code)).call(document.body);"
		"  }"
		"})();";

	if (!cres || !qjs_is_running(cres))
		return;

	bow_reEval_js(cres, onload_script);
}

/*
 * StackAction
 */
void StackAction(BowStack cs, BowRequest *wr, char *action) {
	MemPool mp;
	char *rstr;
	char *rname;
	size_t len;
	bool newhead;

	if (wr == NULL) return;

	/* 1. Intercept synthetic DOM events (<a href="bow-event:click?node=0x...">) */
	if (wr->scheme != NULL && strcmp(wr->scheme, "bow-event") == 0) {
		void *ptr = NULL;
		if (wr->url != NULL &&
		    strncmp(wr->url, "bow-event:click?node=", 21) == 0 &&
		    sscanf(wr->url + 21, "%p", &ptr) == 1 && ptr != NULL) {
			BowDispatchDOMClick(cs->wc->cres, (HtmlDom_t *)ptr);
		}
		RequestDestroy(wr);
		return;
	}

	/* 2. Intercept javascript: pseudo-URLs (<a href="javascript:...">) */
	if (wr->scheme != NULL && strcmp(wr->scheme, "javascript") == 0) {
		if (wr->url != NULL && strncasecmp(wr->url, "javascript:", 11) == 0) {
			MemPool jmp = MPCreate();
			char *code = decode_js_uri(jmp, wr->url + 11);
			if (code != NULL && *code != '\0') {
				bow_reEval_js(cs->wc->cres, code);
			}
			MPDestroy(jmp);
		}

		RequestDestroy(wr);
		return;
	}

	if (strcmp("open", action) == 0) {
		mp = MPCreate();
		len = strlen(wr->scheme) + strlen(".newhead");
		rname = (char *)MPGet(mp, len + 1);
		strncpy(rname, wr->scheme, len);
		strncat(rname, ".newhead", len);
		rname[len] = '\0';
		if ((rstr = ResourceGetBool(cs->wc->cres, rname, &newhead)) == NULL ||
			!newhead) {
			StackOpen(cs, wr);
		} else {
			HeadCreate(cs->wc->cres, wr, NULL);
		}
		MPDestroy(mp);
	} else if (strcmp("download", action) == 0) DownloadOpen(cs->wc->cres, wr);
	else if (strcmp("external", action) == 0) ViewOpen(cs->wc->cres, wr);
	else fprintf(stderr, "Action cannot be taken.  Bug.\n");
}

/*
 * StackRenderAction
 */
static void StackRenderAction(void *closure, BowRender wn, BowRequest *wr, char *action) {
	StackAction((BowStack)closure, wr, action);
}

/*
 * StackSinkInit
 */
int StackSinkInit(BowSink wp, void *closure) {
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
	else {
		if ((rh = RenderGetHooks(cs->wc->cres, ctype)) == NULL) {
			if ((wr = RequestCreate(cs->wc->cres, url, NULL)) == NULL) {
				HeadPrintMessage(cs->wc, "Invalid URL.");
			} else ViewOpen(cs->wc->cres, wr);
			SinkDestroy(wp);
			cs->pending = NULL;
			return(-1);
		}
	}

	if (cs->wn != NULL) {
		curstate = RenderGetState(cs->wn);
		GUIReset(cs->wd);
		RenderDestroy(cs->wn);
		cs->wn = NULL;
	} else curstate = NULL;

	if (cs->wp != NULL) SinkDestroy(cs->wp);
	cs->wp = cs->pending;
	cs->pending = NULL;

	if (cs->action == StackBackAction) {
		StackPop(cs);
		u = (URLInfo *)GListGetHead(cs->urls);
		newstate = u->state;
		StackPop(cs);
	} else if (cs->action == StackReloadAction ||
		cs->action == StackReplaceAction) {
		u = (URLInfo *)GListGetHead(cs->urls);
		newstate = u->state;
		StackPop(cs);
	} else if (cs->action == StackHomeAction) {
		u = (URLInfo *)GListGetTail(cs->urls);
		newstate = u->state;
		while (StackPop(cs))
			;
		MPDestroy(cs->tmp);
		cs->tmp = MPCreate();
	} else {
		if ((u = (URLInfo *)GListGetHead(cs->urls)) != NULL) {
			u->state = curstate;
		}
		newstate = NULL;
	}

	if ((u = (URLInfo *)GListPop(cs->ourls)) == NULL) {
		u = (URLInfo *)MPCGet(cs->mp, sizeof(URLInfo));
	}
	u->url = MPStrDup(cs->tmp, url);
	u->state = newstate;

	GListAddHead(cs->urls, u);

	if ((cs->wn = RenderCreate(cs->wc, cs->wd,
		cs->wp, rh, NULL, NULL, u->state,
		cs->rap, cs->rapc)) == NULL) {
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
char *StackGetCurrentURL(BowStack cs) {
	URLInfo *u;

	if ((u = (URLInfo *)GListGetHead(cs->urls)) == NULL) return(NULL);
	return(u->url);
}

/*
 * StackOp
 */
static void StackOp(BowStack cs, BowRequest *wr, StackOpAction action) {
	BowSinkHooks hooks;

	if (wr == NULL) {
		HeadPrintMessage(cs->wc, "Invalid URL");
		return;
	}

	StackCancel(cs);
	if (cs->pending != NULL) {
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

	if (wr->url != NULL && cs->wc->cres->logfp != NULL) {
		fprintf(cs->wc->cres->logfp, "%s\n", wr->url);
	}

	cs->pending = SinkCreate(cs->wc->cres, wr);
	if (cs->pending != NULL) SinkSetHooks(cs->pending, &hooks, cs);
}

/*
 * StackGUIResize
 */
static void StackGUIResize(BowGUI wd, void *closure, unsigned int  width, unsigned int  height) {
	StackRedraw((BowStack)closure);
}

/*
 * StackCreateToplevel
 */
BowStack StackCreateToplevel(BowContext wc, Widget widget) {
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
BowStack StackCreate(BowStack parent, int x, int y, unsigned int width,
	unsigned int height, BowRenderActionProc rap, void *rapc) {
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
	if (cs->rap == NULL || cs->rapc == NULL) {
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
BowRender StackToRender(BowStack cs) {
	return(cs->wn);
}

/*
 * StackToGUI
 */
BowGUI StackToGUI(BowStack cs) {
	return(cs->wd);
}

/*
 * StackSetRender
 */
void StackSetRender(BowStack cs, BowRenderHooks *rh) {
	cs->rh = rh;
}

/*
 * StackDestroy
 */
void StackDestroy(BowStack cs) {
	if (cs->pending != NULL) SinkDestroy(cs->pending);
	if (cs->wp != NULL) SinkDestroy(cs->wp);
	if (cs->wn != NULL) RenderDestroy(cs->wn);
	if (cs->wd != NULL) GUIDestroy(cs->wd);
	if (cs->tmp != NULL) MPDestroy(cs->tmp);
	GListRemoveItem(cs->wc->cres->stacks, cs);
	MPDestroy(cs->mp);
}

/*
 * StackFromGUI
 */
BowStack StackFromGUI(BowContext wc, BowGUI wd) {
	BowStack cs;

	for (cs = (BowStack)GListGetHead(wc->cres->stacks); cs != NULL;
		cs = (BowStack)GListGetNext(wc->cres->stacks)) {
		if (cs->wd == wd) return(cs);
	}

	return(NULL);
}

/*
 * StackGetParent
 */
BowStack StackGetParent(BowStack cs) {
	return(cs->parent);
}
