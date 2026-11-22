/*
 * module.c
 *
 * libhtml - HTML->X renderer
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

#include <sys/types.h>
#include <sys/stat.h>


#include "html.h"
#include "../bow/BowP.h"
#include "bow_engine.h"

static bool InAnchor(int, int, HTMLBox);
static void HTMLAdd(void *);
static void HTMLEnd(void *);
static bool HTMLExpose(void *, int, int,
	unsigned int, unsigned int);
static bool HTMLMotion(void *, int, int);
static bool HTMLMouse(void *, int, int, char *);
static byte *HTMLQuery(void *, char *);
static int HTMLSearch(void *, char *, int);
static void HTMLDestroy(void *);
static void HTMLCancel(void *);
static void *HTMLInit(BowRender, void *, void *);
static void *HTMLGetState(void *);
static void HTMLClassDestroy(void *);
int InitModule_HTML(BowResources);
static void DeselectTimeOut(BowTimeOut, void *);

static void DeselectTimeOut(BowTimeOut wt, void *closure) {
	HTMLAnchor a;
	HTMLInfo li = (HTMLInfo)closure;

	for (a = (HTMLAnchor)GListGetHead(li->alist); a != NULL;
		a = (HTMLAnchor)GListGetNext(li->alist)) {
		if (a->p == li->sa->p) {
			HTMLClearB(a->box, BOX_SELECT);
			HTMLRenderBox(li, NULL, a->box);
		}
	}

	return;
}

static void HTMLAdd(void *closure) {
	HTMLInfo li = (HTMLInfo)closure;
	byte *data;
	size_t len;
	MIMEHeader mh;

	if (li->cancel) return;

	SinkGetData(li->wp, &data, &len, &mh);

	/*
	 * Partial chunks: bow_engine_add transparently forwards to MLAddData.
	 * The HTML5 pipeline is deferred until the final chunk in HTMLEnd.
	 */
	bow_engine_add(li->hs, (char *)data, len);
}

static void HTMLEnd(void *closure) {
	HTMLInfo li = (HTMLInfo)closure;
	byte *data;
	size_t len;
	MIMEHeader mh;

	if (li->cancel) return;

	SinkGetData(li->wp, &data, &len, &mh);
	li->cres->active_html = li;
	bow_engine_end(li->hs, li->cres, (char *)data, len);

	/* mark this HTMLInfo as active before running engine */
	li->cres->active_html = (void *)li;

	/*
	 * Final chunk: bow_engine_end detects the document type and routes:
	 *   HTML5 / scripted → html_parse + JS + dom_serialize → MLEndData
	 *   Legacy HTML 3.2  → MLEndData directly (unchanged behaviour)
	 */
	bow_engine_end(li->hs, li->cres, (char *)data, len);
}

static bool HTMLExpose(void *closure, int x, int y, unsigned int width, unsigned int height) {
	HTMLInfo li = (HTMLInfo)closure;
	Region r;
	XRectangle rec;

	r = XCreateRegion();
	rec.x = (short)x;
	rec.y = (short)y;
	rec.width = (unsigned short)width;
	rec.height = (unsigned short)height;
	XUnionRectWithRegion(&rec, r, r);

	HTMLRenderBox(li, r, li->firstbox);

	XDestroyRegion(r);

	return(true);
}

/*
 * InAnchor
 */
static bool InAnchor(int x, int y, HTMLBox box) {
	if (x > box->x &&
		x < box->x + box->width &&
		y > box->y &&
		y < box->y + box->height) {
		return(true);
	}

	return(false);
}

static bool HTMLMouse(void *closure, int x, int y, char *action) {
	HTMLInfo li = (HTMLInfo)closure;
	HTMLAnchor a;
	GList list;

	list = li->alist;
	for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
		a = (HTMLAnchor)GListGetNext(list)) {
		if (InAnchor(x, y, a->box)) {
			if (a->p != NULL) {
				li->sa = a;
				for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
					a = (HTMLAnchor)GListGetNext(list)) {
					if (a->p == li->sa->p) {
						HTMLSetB(a->box, BOX_SELECT);
						HTMLRenderBox(li, NULL, a->box);
					}
				}
				XFlush(li->dpy);
				HTMLLoadAnchor(li, li->sa, x, y, action, false);

				if (li->sto != NULL) TimeOutDestroy(li->sto);
				li->sto = TimeOutCreate(li->cres, li->selectTimeOut,
					DeselectTimeOut, li);
			}
			break;
		}
	}

	return(true);
}

static bool HTMLMotion(void *closure, int x, int y) {
	HTMLInfo li = (HTMLInfo)closure;
	HTMLAnchor a;
	GList list;

	list = li->alist;
	for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
		a = (HTMLAnchor)GListGetNext(list)) {
		if (InAnchor(x, y, a->box)) break;
	}

	if (a == NULL || a->p == NULL) RenderSendMessage(li->wn, NULL);
	else HTMLPrintAnchor(li, a, x, y, false);
	li->over = a;

	return(true);
}

/*
 * HTMLQuery
 */
static byte *HTMLQuery(void *closure, char *key) {
	HTMLInfo li = (HTMLInfo)closure;

	if (strcmp(key, "title") == 0) {
		if (li->title != NULL) return(li->title);
	}
	return(NULL);
}

/*
 * HTMLSearch
 *
 * And here's an interesting development...there is no way to do a
 * search for text unless another field is added to HTMLBox.  That
 * is unsavory.  Work on this later.
 */
static int HTMLSearch(void *closure, char *key, int mode) {
	return(-1);
}

/*
 * HTMLCancel
 */
static void HTMLCancel(void *closure) {
	HTMLInfo li = (HTMLInfo)closure;
	BowSink wp;

	myassert(li != NULL, "HTMLInfo is NULL!");

	for (wp = (BowSink)GListGetHead(li->sinks); wp != NULL;
		wp = (BowSink)GListGetNext(li->sinks)) {
		SinkCancel(wp);
	}

	li->cancel = true;

	if (li->wt != NULL) {
		TaskRemove(li->cres, li->wt);
		li->wt = NULL;
	}
}

/*
 * HTMLDestroy
 */
static void HTMLDestroy(void *closure) {
	HTMLInfo li = (HTMLInfo)closure;
	BowSink wp;

  /* clear active_html if pointing*/
	if (li->cres != NULL && li->cres->active_html == li)
		li->cres->active_html = NULL;

	GListRemoveItem(li->lc->contexts, li);
	HTMLDestroyBox(li, li->firstbox);

	while ((wp = (BowSink)GListPop(li->sinks)) != NULL) {
		SinkDestroy(wp);
	}

	HTMLDestroyFrameSets(li);

	if (li->wt != NULL) TaskRemove(li->cres, li->wt);
	if (li->sto != NULL) TimeOutDestroy(li->sto);
	if (li->hs != NULL) MLDestroy(li->hs);

	MPDestroy(li->mp);
}

/*
 * HTMLInit
 */
static void *HTMLInit(BowRender wn, void *closure, void *state) {
	HTMLInfo li;
	HTMLClass lc = (HTMLClass)closure;
	MemPool mp;
	char *url;
	BowSink wp;
	BowGUI wd;

	wp = RenderToSink(wn);
	wd = RenderToGUI(wn);

	url = SinkGetInfo(wp, "x-url");

	mp = MPCreate();
	li = (HTMLInfo)MPCGet(mp, sizeof(struct HTMLInfoP));
	li->mp = mp;
	li->wc = RenderToContext(wn);
	li->cres = RenderToResources(wn);
	li->cres->active_html = NULL;
	li->ps = (HTMLState)state;
	li->url = url;
	li->burl = url;
	li->wd = wd;
	li->wp = wp;
	li->wn = wn;
	li->widget = GUIToWidget(li->wd);
	li->hs = MLInit(HTMLHandler, li);
	li->lc = lc;
	li->css = lc->css;
	li->envstack = GListCreateX(mp);
	li->selectors = GListCreateX(mp);
	li->oldselectors = GListCreateX(mp);
	li->endstack = GListCreateX(mp);
	li->alist = GListCreateX(mp);
	li->maps = GListCreateX(mp);
	li->sinks = GListCreateX(mp);
	li->loads = GListCreateX(mp);
	li->framesets = GListCreateX(mp);

	if (SinkWasReloaded(wp)) li->reload = true;
	else li->reload = false;

	ResourceGetInt(li->cres, "html.leftMargin", &(li->lmargin));
	if (li->lmargin <= 0) li->lmargin = 20;

	ResourceGetInt(li->cres, "html.rightMargin", &(li->rmargin));
	if (li->rmargin <= 0) li->rmargin = 20;

	ResourceGetInt(li->cres, "html.bottomMargin", &(li->bmargin));
	if (li->bmargin <= 0) li->bmargin = 20;

	ResourceGetInt(li->cres, "html.topMargin", &(li->tmargin));
	if (li->tmargin <= 0) li->tmargin = 20;

	ResourceGetBool(li->cres, "html.printHTMLErrors", &(li->printHTMLErrors));

	li->textLineSpace = -1;
	ResourceGetInt(li->cres, "html.textLineSpace", &(li->textLineSpace));
	if (li->textLineSpace < 0) li->textLineSpace = 3;

	li->tableCellInfinity = 0;
	ResourceGetUInt(li->cres,
		"html.tableCellInfinity", &(li->tableCellInfinity));

	ResourceGetUInt(li->cres, "html.inlineTimeOut", &(li->inlineTimeOut));
	if (li->inlineTimeOut < 5) li->inlineTimeOut = 5000;

	ResourceGetUInt(li->cres, "html.selectTimeOut", &(li->selectTimeOut));
	if (li->selectTimeOut < 1000) li->selectTimeOut = 1000;

	ResourceGetBool(li->cres, "html.flowDebug", &(li->flowDebug));

	ResourceGetBool(li->cres, "html.constraintDebug", &(li->constraintDebug));

	ResourceGetBool(li->cres, "html.printTags", &(li->printTags));

	/*
	 * Graphics setup
	 */
	li->dpy = GUIToDisplay(li->wd);
	li->win = GUIToWindow(li->wd);
	li->gc = DefaultGC(li->dpy, DefaultScreen(li->dpy));

	GUIGetNamedColor(wd, "blue", &(li->anchor_color));
	GUIGetNamedColor(wd, "red", &(li->anchor_select_color));
	GUIGetNamedColor(wd, "black", &(li->fg));

	XSetForeground(li->dpy, li->gc, li->fg);

	HTMLSetupFonts(li);

	GUISetScrollBar(wd, true);

	if (GUIGetDimensions(wd, &(li->maxwidth), &(li->maxheight)) == -1) {
	  /* Dimensions of display not set so we'll set them. */
		li->maxwidth = 200;
		li->maxheight = 200;
		GUISetDimensions(wd, li->maxwidth, li->maxheight);
		/* Now grab the internal dimensions again */
		if (GUIGetDimensions(wd, &(li->maxwidth), &(li->maxheight)) == -1) {
			return(NULL);
		}
	}

	GListAddHead(lc->contexts, li);

	HTMLStart(li);

	return(li);
}

/*
 * HTMLClassDestroy
 */
static void HTMLClassDestroy(void *closure) {
	HTMLClass lc = (HTMLClass)closure;

	HTMLFreeFonts(lc);
	MPDestroy(lc->mp);

	return;
}

/*
 * HTMLGetState
 */
void *HTMLGetState(void *closure) {
	HTMLInfo li = (HTMLInfo)closure;
	HTMLState ps;

	if ((ps = (HTMLState)GListPop(li->lc->oldstates)) == NULL) {
		ps = (HTMLState)MPCGet(li->lc->mp, sizeof(struct HTMLStateP));
	}

	GUIGetScrollPosition(li->wd, &(ps->x), &(ps->y));

	return(ps);
}

/*
 * InitModule_HTML
 */
int InitModule_HTML(BowResources cres) {
	BowRenderHooks rh;
	HTMLClass lc;
	MemPool mp;
	FILE *fp;
	char *cssfile;
	char *b;
	struct stat st;
	size_t rval, blen;

	mp = MPCreate();
	lc = (HTMLClass)MPCGet(mp, sizeof(struct HTMLClassP));
	lc->mp = mp;
	lc->oldstates = GListCreateX(mp);
	lc->contexts = GListCreateX(mp);

	/*
	 * Snarf up the default style sheet.
	 */
	if ((cssfile = ResourceGetFilename(cres, mp, "html.cssFile")) != NULL) {
		if (stat(cssfile, &st) == 0 && (fp = fopen(cssfile, "r")) != NULL) {
			if ((b = (char *)malloc(st.st_size)) == NULL) {
				while ((rval = fread(b + blen, 1, st.st_size - blen, fp)) > 0 &&
					!feof(fp)) {
					blen += rval;
				}
				fclose(fp);

				if (blen == st.st_size) {
					lc->css = CSSParseBuffer(NULL, b, blen, NULL, NULL);
				}

				free(b);
			}
		}
	}

	memset(&rh, 0, sizeof(rh));
	rh.class_context = lc;
	rh.class_destroy = HTMLClassDestroy;
	rh.content = "text/html";
	rh.init = HTMLInit;
	rh.add = HTMLAdd;
	rh.end = HTMLEnd;
	rh.search = HTMLSearch;
	rh.destroy = HTMLDestroy;
	rh.getstate = HTMLGetState;
	rh.query = HTMLQuery;
	rh.cancel = HTMLCancel;
	rh.expose = HTMLExpose;
	rh.select = HTMLMouse;
	rh.motion = HTMLMotion;
	RenderAddHooks(cres, &rh);

	return(0);
}

/*
 * HTMLSetFinalPosition
 */
void HTMLSetFinalPosition(HTMLInfo li) {
	char *cp;

	if (li->ps != NULL) {
		GUISetScrollPosition(li->wd, li->ps->x, li->ps->y);
		GListAddHead(li->lc->oldstates, li->ps);
		li->ps = NULL;
	} else {
		for (cp = li->url; *cp != '\0'; cp++) {
			if (*cp == '#') {
				HTMLFindName(li, cp + 1);
				return;
			}
		}
	}
}

void HTMLRepaintFromDOM(HTMLInfo li) {

	if (li == NULL || li->cres == NULL || li->cancel)
		return;


	char *snapshot = bow_engine_get_dom_snapshot(li->cres);
	if (snapshot == NULL) {
		return;
	}

	/* Preserve current scroll position across repaint */
	if (li->ps == NULL) {
		if ((li->ps = (HTMLState)GListPop(li->lc->oldstates)) == NULL)
			li->ps = (HTMLState)MPCGet(li->lc->mp, sizeof(struct HTMLStateP));
		GUIGetScrollPosition(li->wd, &(li->ps->x), &(li->ps->y));
	}

	/* Tear down old boxes and ML scanner */
	if (li->firstbox != NULL) {
		HTMLDestroyBox(li, li->firstbox);
		li->firstbox = NULL;
	}
	if (li->hs != NULL) {
		MLDestroy(li->hs);
		li->hs = NULL;
	}

	/* Reset layout stacks and re-initialize ML with exact signature from HTMLInit */
	li->topenv = NULL;
	li->delayed = 0;
	li->sa = NULL;
	li->over = NULL;
	if (li->envstack != NULL) GListDestroy(li->envstack);
	if (li->endstack != NULL) GListDestroy(li->endstack);
	if (li->alist != NULL)    GListDestroy(li->alist);
	li->envstack = GListCreateX(li->mp);
	li->endstack = GListCreateX(li->mp);
	li->alist = GListCreateX(li->mp);

	li->is_repainting = true;
	li->hs = MLInit(HTMLHandler, li);
	HTMLStart(li);

  /* Feed the full serialized DOM buffer directly into MLEndData */
	MLEndData(li->hs, snapshot, strlen(snapshot));
	li->is_repainting = false;

	free(snapshot);

	/* Restore scroll position and trigger X11 Expose repaint */
	HTMLSetFinalPosition(li);
	if (li->dpy != NULL && li->win != None) {
		XClearArea(li->dpy, li->win, 0, 0, 0, 0, True);
		XFlush(li->dpy);
	}
}
