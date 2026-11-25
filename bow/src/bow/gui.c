/*
 * gui.c
 */

#include <stdio.h>
#include <string.h>

#include "BowP.h"

#include "WWWP.h"

struct BowGUIP {
	Widget             www;                    /* Xt widget */
	Widget             viewport;               /* Enclosing Xaw95 Viewport widget (if present) */
	Widget             vp_clip;                /* Enclosing Viewport's internal clip widget */
	MemPool            mp;                     /* memory pool */
	BowContext     wc;
	BowRender      wn;

	/* state flags */
	bool               position_set;

	bool               size_set;
	unsigned int       width, height;

	GUISizeCallback    size_callback;
	void *size_closure;

	BowTask        resize_task;
};

static void ResizeTask(void *);
void GUIToplevelResize(Widget, void *, void *);
static void GUIViewportResizeEH(Widget, XtPointer, XEvent *, Boolean *);
static void GUIExpose(Widget, void *, int, int, unsigned int, unsigned int);
static void GUIMotion(Widget, void *, int, int, int);
static void GUISelect(Widget, void *, int, int, int);

/*
 * Helper: query visible viewport clip size when embedded inside an outer Viewport
 */
static void GetVisibleViewportSize(BowGUI wd, unsigned int *width, unsigned int *height) {
	Widget clip = (wd->vp_clip != NULL) ? wd->vp_clip : XtParent(wd->www);

	if (wd->viewport != NULL && clip != NULL) {
		*width = (unsigned int)clip->core.width;
		*height = (unsigned int)clip->core.height;
	} else {
		*width = (unsigned int)wd->www->core.width;
		*height = (unsigned int)wd->www->core.height;
	}
}

/*
 * Event handler for window and scrollbar resizes on the Viewport's clip widget.
 * Defers relayout via TaskSchedule so Xt finishes resizing child X windows first,
 * preventing mid-resize glyph corruption and clipping behind the right scrollbar.
 */
static void GUIViewportResizeEH(Widget w, XtPointer closure, XEvent *event, Boolean *cont) {
	BowGUI wd = (BowGUI)closure;

	(void)w;
	(void)cont;

	if (event->type == ConfigureNotify) {
		unsigned int width, height;

		GetVisibleViewportSize(wd, &width, &height);
		if (width == 0 || height == 0) return;

		if (wd->width != width || wd->height != height) {
			wd->width = width;
			wd->height = height;

			if (wd->resize_task != NULL) {
				TaskRemove(wd->wc->cres, wd->resize_task);
				wd->resize_task = NULL;
			}
			wd->resize_task = TaskSchedule(wd->wc->cres, ResizeTask, wd);
		}
	}
}

/*
 * GUIAddRender
 */
void GUIAddRender(BowGUI wd, BowRender wn) {
	wd->wn = wn;
	return;
}

/*
 * GUICreate
 */
BowGUI GUICreate(BowContext wc, BowGUI parent,
	GUISizeCallback size_callback, void *size_closure) {
	BowGUI wd;
	MemPool mp;

	myassert(parent != NULL, "NULL parent not allowed");

	mp = MPCreate();
	wd = (BowGUI)MPCGet(mp, sizeof(struct BowGUIP));
	wd->mp = mp;
	wd->wc = wc;
	wd->size_set = false;
	wd->position_set = false;
	wd->size_callback = size_callback;
	wd->size_closure = size_closure;

	wd->www = XtVaCreateManagedWidget("www_child",
		wwwWidgetClass,
		WWWGetDrawWidget(parent->www),
		XtNmappedWhenManaged, False,
		NULL);

	WWWSetSelectCallback(wd->www, GUISelect, wd);
	WWWSetMotionCallback(wd->www, GUIMotion, wd);
	WWWSetExposeCallback(wd->www, GUIExpose, wd);

	return(wd);
}

/*
 * GUIDestroy
 */
void GUIDestroy(BowGUI wd) {
	MemPool mp = wd->mp;

	if (wd->vp_clip != NULL) {
		XtRemoveEventHandler(wd->vp_clip, StructureNotifyMask, False,
			GUIViewportResizeEH, (XtPointer)wd);
	}
	if (wd->www != NULL) XtDestroyWidget(wd->www);
	if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);
	memset(wd, 0, sizeof(struct BowGUIP));
	MPDestroy(mp);
}

/*
 * GUIToWindow
 */
Window GUIToWindow(BowGUI wd) {
	return(XtWindow(WWWGetDrawWidget(wd->www)));
}

/*
 * GUIToDisplay
 */
Display *GUIToDisplay(BowGUI wd) {
	return(XtDisplay(wd->www));
}

/*
 * GUICreateToplevel
 */
BowGUI GUICreateToplevel(BowContext wc, Widget parent,
	GUISizeCallback size_callback, void *size_closure) {
	BowGUI wd;
	MemPool mp;

	mp = MPCreate();
	wd = (BowGUI)MPCGet(mp, sizeof(struct BowGUIP));
	wd->mp = mp;
	wd->wc = wc;
	wd->viewport = parent;
	wd->size_set = true;
	wd->position_set = true;
	wd->size_callback = size_callback;
	wd->size_closure = size_closure;

	wd->www = XtVaCreateManagedWidget("www_toplevel",
		wwwWidgetClass, parent, XtNwidth, wd->wc->athena->width, XtNheight, wd->wc->athena->height, NULL);

	/*
	 * Viewport reparents its managed child into an internal "clip" widget.
	 * Listening to ConfigureNotify on vp_clip catches both outer window resizes
	 * AND inner width changes when the right vertical scrollbar maps/unmaps.
	 */
	wd->vp_clip = XtParent(wd->www);
	if (wd->vp_clip != NULL) {
		XtAddEventHandler(wd->vp_clip, StructureNotifyMask, False,
			GUIViewportResizeEH, (XtPointer)wd);
	}

	WWWSetResizeCallback(wd->www, GUIToplevelResize, wd);
	WWWSetSelectCallback(wd->www, GUISelect, wd);
	WWWSetMotionCallback(wd->www, GUIMotion, wd);
	WWWSetExposeCallback(wd->www, GUIExpose, wd);

	return(wd);
}

/*
 * GUISetScrollBar
 */
void GUISetScrollBar(BowGUI wd, bool use_scroll) {
	WWWSetScrollBar(wd->www, use_scroll);
}

/*
 * GUIGetDimenions
 */
int GUIGetDimensions(BowGUI wd, unsigned int *width, unsigned int *height) {
	if (!wd->size_set) return(-1);
	if (wd->viewport != NULL) {
		GetVisibleViewportSize(wd, width, height);
	} else {
		WWWGetDrawSize(wd->www, width, height);
	}
	return(0);
}

/*
 * GUISetDimensions
 */
void GUISetDimensions(BowGUI wd, unsigned int width, unsigned int height) {
	WWWSetDrawSize(wd->www, width, height);
}

int GUIGetNamedColor(BowGUI wd, char *name, Pixel *_pixel) {
	XColor sxc, exc;
	Display *dpy = XtDisplay(wd->www);

	XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
		name, &sxc, &exc);

	*_pixel = sxc.pixel;

	return(0);
}

/*
 * GUIGetOnScreenDimensions
 */
void GUIGetOnScreenDimensions(BowGUI wd, int *x, int *y, unsigned int *width, unsigned int *height) {
	WWWWidget rw = (WWWWidget)wd->www;
	Widget clip = (wd->vp_clip != NULL) ? wd->vp_clip : XtParent(wd->www);

	if (wd->viewport != NULL && clip != NULL) {
		*x = -(int)wd->www->core.x;
		*y = -(int)wd->www->core.y;
		*width = (unsigned int)clip->core.width;
		*height = (unsigned int)clip->core.height;
		return;
	}

	*x = -(int)rw->www.child->core.x;
	*y = -(int)rw->www.child->core.y;
	*width = (unsigned int)rw->www.child->core.width;
	*height = (unsigned int)rw->www.clip->core.height;
}

/*
 * GUIReset
 */
void GUIReset(BowGUI wd) {
	WWWWidget rw = (WWWWidget)wd->www;
	unsigned int vis_w, vis_h;

	wd->wn = NULL;
	XClearWindow(XtDisplay(rw->www.child), XtWindow(rw->www.child));
	WWWMoveChild(wd->www, 0, 0);
	GetVisibleViewportSize(wd, &vis_w, &vis_h);
	WWWSetDrawSize(wd->www, vis_w, vis_h);
}

/*
 * GUIExpose
 */
static void GUIExpose(Widget w, void *closure, int x, int y,
	unsigned int width, unsigned int  height) {
	BowGUI wd = (BowGUI)closure;

	if (wd->wn != NULL) RenderExpose(wd->wn, x, y, width, height);
}

/*
 * GUISelect
 */
static void GUISelect(Widget w, void *closure, int x, int y, int button) {
	BowGUI wd = (BowGUI)closure;
	char *action;

	if (wd->wn != NULL) {
		if (button == 1) action = "open";
		else if (button == 2) action = "download";
		else if (button == 3) action = "external";
		else action = "open";

		RenderSelect(wd->wn, x, y, action);
	}
}

/*
 * GUIMotion
 */
static void GUIMotion(Widget w, void *closure, int x, int y, int button) {
	BowGUI wd = (BowGUI)closure;

	if (wd->wn != NULL) RenderMotion(wd->wn, x, y);
}

/*
 * GUIToplevelResize
 */
void GUIToplevelResize(Widget w, void *closure, void *junk) {
	BowGUI wd = (BowGUI)closure;

	(void)w;
	(void)junk;

	if (wd->size_callback != NULL) {
		unsigned int width, height;

		GetVisibleViewportSize(wd, &width, &height);
		if (width == 0 || height == 0) return;

		if (wd->width != width || wd->height != height) {
			wd->width = width;
			wd->height = height;

			if (wd->resize_task != NULL) {
				TaskRemove(wd->wc->cres, wd->resize_task);
				wd->resize_task = NULL;
			}
			wd->resize_task = TaskSchedule(wd->wc->cres, ResizeTask, wd);
		}
	}
}

/*
 * GUIMap
 */
void GUIMap(BowGUI wd, int x, int y) {
	if (!wd->size_set) {
		fprintf(stderr, "GUIMap: GUI dimensions not set yet.\n");
		return;
	}

	if (wd->position_set) {
		fprintf(stderr, "GUIMap: already mapped.\n");
		return;
	}

	XtConfigureWidget(wd->www, (Position)x, (Position)y,
		(Dimension)wd->width, (Dimension)wd->height, 0);

	XtSetMappedWhenManaged(wd->www, True);
}

/*
 * GUIUnmap
 */
void GUIUnmap(BowGUI wd) {
	XtSetMappedWhenManaged(wd->www, False);
	wd->size_set = false;
	wd->position_set = false;
}

/*
 * ResizeTask
 */
static void ResizeTask(void *closure) {
	BowGUI wd = (BowGUI)closure;
	WWWWidget rw = (WWWWidget)wd->www;

	wd->resize_task = NULL;

	if (rw != NULL && rw->www.child != NULL &&
		XtIsRealized(rw->www.child) && XtWindow(rw->www.child) != None) {
		XClearWindow(XtDisplay(rw->www.child), XtWindow(rw->www.child));
	}

	if (wd->size_callback != NULL) {
		CMethod(wd->size_callback)(wd, wd->size_closure, wd->width, wd->height);
	}
}

/*
 * GUISetInitialDimensions
 */
void GUISetInitialDimensions(BowGUI wd, unsigned int width, unsigned int height) {
	myassert(!wd->size_set, "GUISetInitialDimensions: dimensions already set");

	if (wd->resize_task != NULL) TaskRemove(wd->wc->cres, wd->resize_task);

	wd->width = width;
	wd->height = height;
	wd->size_set = true;

	XtResizeWidget(wd->www, (Dimension)width, (Dimension)height, 0);
	WWWSetDrawSize(wd->www, width, height);

	wd->resize_task = TaskSchedule(wd->wc->cres, ResizeTask, wd);
}

/*
 * GUISetScrollPosition
 */
void GUISetScrollPosition(BowGUI wd, int x, int y) {
	WWWMoveChild(wd->www, x, y);
}

/*
 * GUIGetScrollPosition
 */
void GUIGetScrollPosition(BowGUI wd, int *x, int *y) {
	WWWGetScrollPosition(wd->www, x, y);
}

/*
 * GUIBackgroundPixel
 */
Pixel GUIBackgroundPixel(BowGUI wd) {
	Pixel bg;
	XtVaGetValues(wd->www, XtNbackground, &bg, NULL);
	return(bg);
}

/*
 * GUIToWidget
 */
Widget GUIToWidget(BowGUI wd) {
	return(WWWGetDrawWidget(wd->www));
}
