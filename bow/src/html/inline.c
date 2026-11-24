/*
 * inline.c
 *
 * libhtml5 - HTML->X renderer
 */


#include <stdio.h>


#include "html.h"
#include "../bow/javascriptP.h"

struct HTMLInlineP {
	HTMLInfo li;

	BowGUI wd;
	BowSink wp;
	BowRender wn;
	HTMLInlineInfo ii;

	bool setupcalled;

	HTMLBox box;

	unsigned int width, height, border;
	unsigned int vspace, hspace;
	char *mapurl;
	bool ismap;
	HTMLEnv env;
	bool delayed;

	BowRenderHooks orh;
	void *rc;
};

typedef struct {
	HTMLInfo     li;
	HTMLBox      box;
	unsigned int width;
	unsigned int height;
	Pixmap       pixmap;
	GC           gc;
} HTMLCanvasData;

/* track active canvas box so 2D context can draw at 60fps */
static HTMLCanvasData *g_active_canvas = NULL;

static int ImgInit(BowSink, void *);
static void ImgAdd(void *);
static void ImgEnd(void *);
static void ImgMessage(void *, char *);

static void DestroyInline(HTMLInfo, HTMLBox);
static bool InlineSelectCallback(void *, int, int, char *);
static bool InlineMotionCallback(void *, int, int);

void InlineResizeCallback(BowGUI, void *, unsigned int, unsigned int);

static void InlineContinue(HTMLInline);

static void SetupInline(HTMLInfo, HTMLBox);
static void AddInline(HTMLInline);

static void HandleInlineReload(HTMLInfo, BowRequest *);

/*
 * InlineContinue
 */
static void InlineContinue(HTMLInline isp){
	if (isp->delayed) {
		isp->delayed = false;
		HTMLContinueLayout(isp->li);
	}
	return;
}

/*
 * SetupInline
 */
static void SetupInline(HTMLInfo li, HTMLBox box) {
	HTMLInline isp = (HTMLInline)box->closure;

	isp->setupcalled = true;
	if (isp->wd != NULL) {
		GUIMap(isp->wd, box->x + isp->hspace, box->y + isp->vspace);
	}
}

/*
 * DestroyInline
 */
static void DestroyInline(HTMLInfo li, HTMLBox box) {
	HTMLInline isp = (HTMLInline)box->closure;

	if (isp->wn != NULL) {
		RenderDestroy(isp->wn);
		isp->wn = NULL;
	}
	if (isp->wp != NULL) {
		SinkSetHooks(isp->wp, NULL, NULL);
		isp->wp = NULL;
	}
	if (isp->wd != NULL) {
		GUIDestroy(isp->wd);
		isp->wd = NULL;
	}
}

/*
 * AddInline
 */
static void AddInline(HTMLInline isp) {
	HTMLBox box;
	HTMLAttribID aid;
	int border;

	isp->box = box = HTMLCreateBox(isp->li, isp->env);

	if ((border = MLAttributeToInt(isp->ii.p, "border")) < 0) isp->border = 0;
	else isp->border = border;

	aid = HTMLAttributeToID(isp->ii.p, "align");
	if (aid == ATTRIB_MIDDLE) box->baseline = isp->height / 2;
	else if (aid == ATTRIB_TOP) box->baseline = 0;
	else if (aid == ATTRIB_LEFT) HTMLSetB(box, BOX_FLOAT_LEFT);
	else if (aid == ATTRIB_RIGHT) HTMLSetB(box, BOX_FLOAT_RIGHT);
	else box->baseline = isp->height;

	box->setup = SetupInline;
	box->destroy = DestroyInline;
	box->width = isp->width + isp->hspace * 2;
	box->height = isp->height + isp->vspace * 2;
	box->closure = isp;

	HTMLEnvAddBox(isp->li, isp->env, box);
}

/*
 * InlineResizeCallback
 */
void InlineResizeCallback(BowGUI wd, void *closure, unsigned int width, unsigned int height) {
	HTMLInline isp = (HTMLInline)closure;

	if (isp->box == NULL) {
		isp->width = width;
		isp->height = height;
		AddInline(isp);
		InlineContinue(isp);
	}
}

/*
 * InlineSelectCallback
 */
static bool InlineSelectCallback(void *closure, int x, int y, char *action) {
	HTMLInline isp = (HTMLInline)closure;
	HTMLAnchor a;
	GList list;
	char *url;

	myassert(isp->box != NULL, "MouseCallback: box == NULL");

	if (isp->mapurl != NULL) {
		if ((url = HTMLFindMapURL(isp->li, isp->mapurl, x, y)) != NULL) {
			HTMLLoadURL(isp->li, NULL, url, action);
			return(true);
		}
	}

	list = isp->li->alist;
	for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
		a = (HTMLAnchor)GListGetNext(list)) {
		if (a->box == isp->box) {
			HTMLLoadAnchor(isp->li, a, x, y, action, isp->ismap);
			return(true);
		}
	}

	return(true);
}

static bool InlineMotionCallback(void *closure, int x, int y) {
	HTMLInline isp = (HTMLInline)closure;
	HTMLAnchor a;
	GList list;
	char *url;

	myassert(isp->box != NULL, "MotionCallback: box == NULL");

	if (isp->mapurl != NULL) {
		if ((url = HTMLFindMapURL(isp->li, isp->mapurl, x, y)) != NULL) {
			HTMLPrintURL(isp->li, url);
			return(true);
		}
	}

	isp->li->over = NULL;
	list = isp->li->alist;
	for (a = (HTMLAnchor)GListGetHead(list); a != NULL;
		a = (HTMLAnchor)GListGetNext(list)) {
		if (a->box == isp->box) {
			HTMLPrintAnchor(isp->li, a, x, y, isp->ismap);
			isp->li->over = a;
			return(true);
		}
	}

	return(true);
}

/*
 * ImgMessage
 */
static void ImgMessage(void *closure, char *message) {
	HTMLInline isp = (HTMLInline)closure;

	if (isp->li->wn != NULL) RenderSendMessage(isp->li->wn, message);
	return;
}

/*
 * ImgEnd
 */
static void ImgEnd(void *closure) {
	HTMLInline isp = (HTMLInline)closure;
	RenderEnd(isp->wn);
	return;
}

/*
 * ImgAdd
 */
static void
ImgAdd(closure)
void *closure;
{
	HTMLInline isp = (HTMLInline)closure;
	RenderAdd(isp->wn);
	return;
}

/*
 * ImgInit
 */
static int ImgInit(BowSink wp, void *closure) {
	HTMLInline isp = (HTMLInline)closure;
	BowRenderHooks *rh;
	char *ctype;

	ctype = SinkGetInfo(wp, "content-type");
	if ((rh = RenderGetHooks(SinkToResources(wp), ctype)) == NULL) {
		SinkCancel(wp);
		InlineContinue(isp);
		return(-1);
	}

	isp->wd = GUICreate(isp->li->wc, isp->li->wd, InlineResizeCallback, isp);
	if (isp->width > 0 && isp->height > 0) {
		GUISetInitialDimensions(isp->wd, isp->width, isp->height);
	}
	if (isp->setupcalled) {
		GUIMap(isp->wd, isp->box->x + isp->hspace, isp->box->y + isp->vspace);
	}

	isp->wn = RenderCreate(isp->li->wc, isp->wd, isp->wp,
		rh, &(isp->orh), isp->rc, NULL,
		NULL, NULL);
	if (isp->wn == NULL) {
		SinkCancel(wp);
		return(-1);
	}

	return(0);
}

/*
 *
 * Public functions
 *
 */
HTMLInline HTMLCreateInline(HTMLInfo li, HTMLEnv env, char *url,
	HTMLInlineInfo *ii, BowRenderHooks *orh, void *rc) {
	BowSinkHooks hooks;
	HTMLInline isp;
	BowRequest *wr;
	int width, height, vspace, hspace;

	if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL) {
		return(NULL);
	}

	if (li->reload) HandleInlineReload(li, wr);

	/* Select only image content that we can deal with directly */
	if (RequestAddRegexContent(li->cres, wr, "image/*") <= 0) {
		RequestDestroy(wr);
		return(NULL);
	}

	isp = (HTMLInline)MPCGet(li->mp, sizeof(struct HTMLInlineP));
	memcpy(&(isp->ii), ii, sizeof(HTMLInlineInfo));
	isp->li = li;
	isp->env = env;
	if (orh == NULL) {
		memset(&(isp->orh), 0, sizeof(isp->orh));
		isp->orh.select = InlineSelectCallback;
		isp->orh.motion = InlineMotionCallback;
		isp->rc = isp;
	} else {
		memcpy(&(isp->orh), orh, sizeof(BowRenderHooks));
		isp->rc = rc;
	}

	if ((width = MLAttributeToInt(ii->p, "width")) < 0) isp->width = 0;
	else isp->width = width;
	if ((height = MLAttributeToInt(ii->p, "height")) < 0) isp->height = 0;
	else isp->height = height;
	if ((vspace = MLAttributeToInt(ii->p, "vspace")) < 0) isp->vspace = 0;
	else isp->vspace = vspace;
	if ((hspace = MLAttributeToInt(ii->p, "hspace")) < 0) isp->hspace = 0;
	else isp->hspace = hspace;
	isp->ismap = MLFindAttribute(ii->p, "ismap") != NULL ? true : false;
	isp->mapurl = MLFindAttribute(ii->p, "usemap");

	if (isp->width == 0 || isp->height == 0) {
		isp->delayed = true;
		HTMLDelayLayout(li);
	} else AddInline(isp);

	memset(&hooks, 0, sizeof(hooks));
	hooks.init = ImgInit;
	hooks.add = ImgAdd;
	hooks.end = ImgEnd;
	hooks.message = ImgMessage;

	if ((isp->wp = SinkCreate(li->cres, wr)) == NULL) {
		InlineContinue(isp);
	} else {
		SinkSetHooks(isp->wp, &hooks, isp);
		GListAddHead(li->sinks, isp->wp);
	}

	return(isp);
}

/*
 * HTMLInlineToBox
 */
HTMLBox HTMLInlineToBox(HTMLInline is) {
	return(is->box);
}

/*
 * HTMLInlineToInfo
 */
HTMLInfo HTMLInlineToInfo(HTMLInline is) {
	return(is->li);
}

/*
 * HTMLInlineDestroy
 */
void HTMLInlineDestroy(HTMLInline is) {
}

/*
 * HTMLImg
 */
void HTMLImg(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *url;
	HTMLInlineInfo ii;

	if ((url = MLFindAttribute(p, "src")) == NULL) return;

	memset(&ii, 0, sizeof(HTMLInlineInfo));
	ii.p = p;

	HTMLCreateInline(li, env, url, &ii, NULL, NULL);
}

/*
 * HTMLImgInsert
 *
 * Always OK image insertion for now.  I suppose this is where one
 * could check for conditional image loading.  The idea here is to
 * start the download as soon as possible.
 */
HTMLInsertStatus HTMLImgInsert(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *url;
	BowRequest *wr;
	BowSink wp;

	if ((url = MLFindAttribute(p, "src")) == NULL) return(HTMLInsertOK);

	if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL) {
		return(HTMLInsertOK);
	}

	if (li->reload) HandleInlineReload(li, wr);

	/* Select only image content that we can deal with directly */
	if (RequestAddRegexContent(li->cres, wr, "image/*") <= 0) {
		RequestDestroy(wr);
		return(HTMLInsertOK);
	}

	if ((wp = SinkCreate(li->cres, wr)) != NULL) {
		SinkSetHooks(wp, NULL, NULL);
		GListAddHead(li->sinks, wp);
	}

	return(HTMLInsertOK);
}

/*
 * HandleInlineReload
 */
static void HandleInlineReload(HTMLInfo li, BowRequest *wr) {
	char *xurl;

	for (xurl = (char *)GListGetHead(li->loads); xurl != NULL;
		xurl = (char *)GListGetNext(li->loads)) {
		if (strlen(wr->url) == strlen(xurl) && strcmp(wr->url, xurl) == 0) {
			break;
		}
	}
	if (xurl == NULL) {
		RequestReload(wr, true);
		GListAddHead(li->loads, MPStrDup(li->mp, wr->url));
	}

	return;
}

static void DestroyCanvas(HTMLInfo li, HTMLBox box) {
	HTMLCanvasData *cd = (HTMLCanvasData *)box->closure;

	if (cd == NULL)
		return;

	if (g_active_canvas == cd)
		g_active_canvas = NULL;

	if (cd->gc != NULL) {
		XFreeGC(li->dpy, cd->gc);
		cd->gc = NULL;
	}

	if (cd->pixmap != None) {
		XFreePixmap(li->dpy, cd->pixmap);
		cd->pixmap = None;
	}
}

static void RenderCanvas(HTMLInfo li, HTMLBox box, Region r) {
	HTMLCanvasData *cd = (HTMLCanvasData *)box->closure;
	int screen;

	(void)r;
	if (cd == NULL || li->dpy == NULL || li->win == None)
		return;

	if (cd->pixmap != None) {
		XCopyArea(li->dpy, cd->pixmap, li->win, li->gc,
			0, 0, cd->width, cd->height, box->x, box->y);
	}

	/* Draw 1px border so the canvas box is visible */
	screen = DefaultScreen(li->dpy);
	XSetForeground(li->dpy, li->gc, BlackPixel(li->dpy, screen));
	if (cd->width > 1 && cd->height > 1) {
		XDrawRectangle(li->dpy, li->win, li->gc,
			box->x, box->y, cd->width - 1, cd->height - 1);
	}
	XSetForeground(li->dpy, li->gc, li->fg);
}

void HTMLCanvas(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLCanvasData *cd;
	HTMLBox box;
	int w, h;
	int screen;

	w = MLAttributeToInt(p, "width");
	if (w <= 0) w = 300; /* HTML5 default width */

	h = MLAttributeToInt(p, "height");
	if (h <= 0) h = 150; /* HTML5 default height */

	cd = (HTMLCanvasData *)MPCGet(li->mp, sizeof(HTMLCanvasData));
	cd->li = li;
	cd->width = (unsigned int)w;
	cd->height = (unsigned int)h;
	g_active_canvas = cd;

	screen = DefaultScreen(li->dpy);
	cd->pixmap = XCreatePixmap(li->dpy, li->win,
		cd->width, cd->height,
		DefaultDepth(li->dpy, screen));
	cd->gc = XCreateGC(li->dpy, cd->pixmap, 0, NULL);
	XSetForeground(li->dpy, cd->gc, WhitePixel(li->dpy, screen));
	XFillRectangle(li->dpy, cd->pixmap, cd->gc, 0, 0, cd->width, cd->height);

	box = HTMLCreateBox(li, env);
	cd->box = box;
	box->width = cd->width;
	box->height = cd->height;
	box->baseline = cd->height;
	box->render = RenderCanvas;
	box->destroy = DestroyCanvas;
	box->closure = cd;
	HTMLEnvAddBox(li, env, box);
	if (li->cres->qjs->canvas) {
		li->cres->qjs->canvas->display = li->dpy;
		li->cres->qjs->canvas->buffer = cd->pixmap;
		li->cres->qjs->canvas->gc = cd->gc;
		li->cres->qjs->canvas->window = li->win;
		li->cres->qjs->canvas->canvas = li->widget;
		li->cres->qjs->canvas->canvas_w = cd->width;
		li->cres->qjs->canvas->canvas_h = cd->height;
	}
}

void HTMLCanvasResize(int w, int h) {
	HTMLCanvasData *cd = g_active_canvas;
	int screen;

	if (cd == NULL || cd->li == NULL || w <= 0 || h <= 0)
		return;

	if ((int)cd->width == w && (int)cd->height == h)
		return;

	screen = DefaultScreen(cd->li->dpy);
	if (cd->pixmap != None)
		XFreePixmap(cd->li->dpy, cd->pixmap);

	cd->width = (unsigned int)w;
	cd->height = (unsigned int)h;
	cd->pixmap = XCreatePixmap(cd->li->dpy, cd->li->win,
		cd->width, cd->height,
		DefaultDepth(cd->li->dpy, screen));
	XSetForeground(cd->li->dpy, cd->gc, WhitePixel(cd->li->dpy, screen));
	XFillRectangle(cd->li->dpy, cd->pixmap, cd->gc, 0, 0, cd->width, cd->height);

	if (cd->box != NULL) {
		cd->box->width = cd->width;
		cd->box->height = cd->height;
		cd->box->baseline = cd->height;
	}

	if (cd->li->cres && cd->li->cres->qjs && cd->li->cres->qjs->canvas) {
		cd->li->cres->qjs->canvas->buffer = cd->pixmap;
		cd->li->cres->qjs->canvas->canvas_w = w;
		cd->li->cres->qjs->canvas->canvas_h = h;
	}

	/* Re-run layout so centering (margin: 0 auto) and surrounding text flow update */
	if (!cd->li->is_repainting) {
		HTMLRepaintFromDOM(cd->li);
	}
}

void HTMLCanvasFillPolygon(XPoint *pts, int npts, const char *color_str) {
	HTMLCanvasData *cd = g_active_canvas;
	unsigned long pixel;

	if (cd == NULL || cd->pixmap == None || npts < 3)
		return;

	if (color_str && GUIGetNamedColor(cd->li->wd, (char *)color_str, &pixel) == 0)
		XSetForeground(cd->li->dpy, cd->gc, pixel);
	else
		XSetForeground(cd->li->dpy, cd->gc, cd->li->fg);

	XFillPolygon(cd->li->dpy, cd->pixmap, cd->gc, pts, npts, Complex, CoordModeOrigin);
}

void HTMLCanvasClearRect(int x, int y, int w, int h) {
	HTMLCanvasData *cd = g_active_canvas;
	int screen;

	if (cd == NULL || cd->pixmap == None)
		return;
	screen = DefaultScreen(cd->li->dpy);
	XSetForeground(cd->li->dpy, cd->gc, WhitePixel(cd->li->dpy, screen));
	XFillRectangle(cd->li->dpy, cd->pixmap, cd->gc, x, y, w, h);
}

void HTMLCanvasStrokeSegments(XSegment *segs, int nsegs, const char *color_str) {
	HTMLCanvasData *cd = g_active_canvas;
	unsigned long pixel;

	if (cd == NULL || cd->pixmap == None || nsegs <= 0)
		return;

	if (color_str && GUIGetNamedColor(cd->li->wd, (char *)color_str, &pixel) == 0)
		XSetForeground(cd->li->dpy, cd->gc, pixel);
	else
		XSetForeground(cd->li->dpy, cd->gc, cd->li->fg);

	XDrawSegments(cd->li->dpy, cd->pixmap, cd->gc, segs, nsegs);
}

void HTMLCanvasFlush(void) {
	HTMLCanvasData *cd = g_active_canvas;
	int screen;

	if (cd == NULL || cd->pixmap == None || cd->box == NULL)
		return;

	/* Fast blit directly to window without re-parsing HTML */
	XCopyArea(cd->li->dpy, cd->pixmap, cd->li->win, cd->li->gc,
		0, 0, cd->width, cd->height, cd->box->x, cd->box->y);

	/* Keep 1px border visible across animation frames */
	screen = DefaultScreen(cd->li->dpy);
	XSetForeground(cd->li->dpy, cd->li->gc, BlackPixel(cd->li->dpy, screen));
	if (cd->width > 1 && cd->height > 1) {
		XDrawRectangle(cd->li->dpy, cd->li->win, cd->li->gc,
			cd->box->x, cd->box->y, cd->width - 1, cd->height - 1);
	}
	XSetForeground(cd->li->dpy, cd->li->gc, cd->li->fg);

	XFlush(cd->li->dpy);
}

void HTMLCanvasGetBounds(int *x, int *y, int *w, int *h) {
	if (g_active_canvas && g_active_canvas->box) {
		if (x) *x = g_active_canvas->box->x;
		if (y) *y = g_active_canvas->box->y;
		if (w) *w = (int)g_active_canvas->width;
		if (h) *h = (int)g_active_canvas->height;
	} else {
		if (x) *x = 0;
		if (y) *y = 0;
		if (w) *w = 300;
		if (h) *h = 150;
	}
}
