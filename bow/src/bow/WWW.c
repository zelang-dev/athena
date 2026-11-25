/*
 * WWW.c
 */

#include <stdio.h>

#include "common.h"
#include <athena.h>

#include <X11/keysym.h>
#include <Xaw95/TraversalP.h>

#include "WWWP.h"
#include "Geom.h"

#define WMethod(x) ((x) != NULL ? (x):10)

/* Guard flag to suppress WWWResize when expanding document size inside Viewport */
static Boolean in_draw_size = False;

/*
 * Action prototypes
 */
static void WWWMotionAction(Widget, XEvent *, String *, Cardinal *);
static void WWWSelectAction(Widget, XEvent *, String *, Cardinal *);
static void WWWExposeAction(Widget, XEvent *, String *, Cardinal *);
static void WWWPageUpAction(Widget, XEvent *, String *, Cardinal *);
static void WWWPageDownAction(Widget, XEvent *, String *, Cardinal *);
static void WWWPageLeftAction(Widget, XEvent *, String *, Cardinal *);
static void WWWPageRightAction(Widget, XEvent *, String *, Cardinal *);
static void WWWScrollUpAction(Widget, XEvent *, String *, Cardinal *);
static void WWWScrollDownAction(Widget, XEvent *, String *, Cardinal *);
static void WWWScrollLeftAction(Widget, XEvent *, String *, Cardinal *);
static void WWWScrollRightAction(Widget, XEvent *, String *, Cardinal *);

static char defaultTranslations[] =
"<Btn1Up>:      select() \n\
 <Btn2Up>:      select() \n\
 <Btn3Up>:      select() \n\
 <Expose>:      expose() \n\
 <Motion>:      motion() \n\
 :<Key>BackSpace: page_up()\n\
 :<Key>Page_Up: page_up()\n\
 :<Key>KP_Page_Up: page_up()\n\
 :<Key>0x20: page_down()\n\
 :<Key>Page_Down: page_down()\n\
 :<Key>KP_Page_Down: page_down()\n\
 :s<Key>Up: page_up()\n\
 :s<Key>KP_Up: page_up()\n\
 :s<Key>Down: page_down()\n\
 :s<Key>KP_Down: page_down()\n\
 :s<Key>Left: page_left()\n\
 :s<Key>KP_Left: page_left()\n\
 :s<Key>Right: page_right()\n\
 :s<Key>KP_Right: page_right()\n\
 :<Key>Up: scroll_up()\n\
 :<Key>KP_Up: scroll_up()\n\
 :<Key>Down: scroll_down()\n\
 :<Key>KP_Down: scroll_down()\n\
 :<Key>Left: scroll_left()\n\
 :<Key>KP_Left: scroll_left()\n\
 :<Key>Right: scroll_right()\n\
 :<Key>KP_Right: scroll_right()\n\
";

static XtActionsRec actionsList[] =
{
  {"expose", WWWExposeAction},
  {"select", WWWSelectAction},
  {"motion", WWWMotionAction},
  {"page_up",    WWWPageUpAction},
  {"page_down",  WWWPageDownAction},
  {"page_left",  WWWPageLeftAction},
  {"page_right", WWWPageRightAction},
  {"scroll_up",    WWWScrollUpAction},
  {"scroll_down",  WWWScrollDownAction},
  {"scroll_left",  WWWScrollLeftAction},
  {"scroll_right", WWWScrollRightAction},
};

#define offset(field) XtOffsetOf(WWWRec, www.field)
static XtResource resources[] =
{
  {XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	offset(background), XtRString, XtDefaultBackground},

  {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(foreground), XtRString, XtDefaultForeground},
};
#undef offset

static void WWWInitialize(Widget, Widget, ArgList, Cardinal *);
void WWWResize(Widget);
static void WWWRealize(Widget, XtValueMask *,
	XSetWindowAttributes *);
static Boolean WWWSetValues(Widget, Widget, Widget,
	ArgList, Cardinal *);
static XtGeometryResult WWWGeometryManager(Widget,
	XtWidgetGeometry *,
	XtWidgetGeometry *);
static XtGeometryResult WWWPreferredGeometry(Widget,
	XtWidgetGeometry *,
	XtWidgetGeometry *);

static void WWWGetValuesHook(Widget, ArgList, Cardinal *);

static Dimension FixDim(unsigned int);

/* Scrollbar handling functions (retained for API/callback compatibility) */
static void RedrawThumbs(WWWWidget);
static void ScrollUpDownProc(Widget, XtPointer, XtPointer);
static void ThumbProc(Widget, XtPointer, XtPointer);

WWWClassRec wwwClassRec =
{
  { /* core_class fields */
		/* superclass         */    (WidgetClass)&compositeClassRec,
		/* class_name         */    "WWW",
		/* widget_size        */    sizeof(WWWRec),
		/* class_initialize   */    NULL,
		/* class_part_init    */    NULL,
		/* class_inited       */    FALSE,
		/* initialize         */    WWWInitialize,
		/* initialize_hook    */    NULL,
		/* realize            */    WWWRealize,
		/* actions            */    actionsList,
		/* num_actions        */    XtNumber(actionsList),
		/* resources          */    resources,
		/* num_resources      */    XtNumber(resources),
		/* xrm_class          */    NULLQUARK,
		/* compress_motion    */    FALSE,
		/* compress_exposure  */    FALSE,
		/* compress_enterleave*/    FALSE,
		/* visible_interest   */    FALSE,
		/* destroy            */    NULL,
		/* resize             */    WWWResize,
		/* expose             */    NULL,
		/* set_values         */    WWWSetValues,
		/* set_values_hook    */    NULL,
		/* set_values_almost  */    XtInheritSetValuesAlmost,
		/* get_values_hook    */    WWWGetValuesHook,
		/* accept_focus       */    XawAcceptFocus,
		/* version            */    XtVersion,
		/* callback_private   */    NULL,
		/* tm_table           */    NULL,
		/* query_geometry     */	WWWPreferredGeometry,
		/* display_accelerator*/	XtInheritDisplayAccelerator,
		/* extension          */	NULL
	  },
	  { /* composite_class fields */
		  /* geometry_manager   */   WWWGeometryManager,
		  /* change_managed     */   NULL,
		  /* insert_child       */   XtInheritInsertChild,
		  /* delete_child       */   XtInheritDeleteChild,
		  /* extension          */   NULL
		},
		{ /* www_class fields */
		  NULL,
		}
};

WidgetClass wwwWidgetClass = (WidgetClass)&wwwClassRec;

/*
 * WWWGetValuesHook
 */
static void WWWGetValuesHook(Widget w, ArgList args, Cardinal *num_args) {
	WWWWidget rw = (WWWWidget)w;
	int i;

	for (i = 0; i < *num_args; i++) {
		if (strcmp(args[i].name, XtNbackground) == 0) {
			*((Pixel *)args[i].value) = rw->www.background;
		}
		if (strcmp(args[i].name, XtNforeground) == 0) {
			*((Pixel *)args[i].value) = rw->www.foreground;
		}
	}
}

/*
 * WWWResize
 */
void WWWResize(Widget w) {
	WWWWidget rw;

	if (XtClass(w) != wwwWidgetClass) return;
	if (in_draw_size) return;

	rw = (WWWWidget)w;
	if (rw->www.resize_callback != NULL) {
		(rw->www.resize_callback)(w, rw->www.resize_closure, NULL);
	}
}

/*
 * WWWRealize
 */
static void WWWRealize(Widget w, XtValueMask *valueMask, XSetWindowAttributes *attributes) {
	(*wwwWidgetClass->core_class.superclass->core_class.realize)
		(w, valueMask, attributes);
}

static XtGeometryResult WWWPreferredGeometry(Widget w, XtWidgetGeometry *con, XtWidgetGeometry *reply) {
	WWWWidget rw = (WWWWidget)w;

	XtResizeWidget(rw->www.clip, con->width > 0 ? con->width : 100,
		con->height > 0 ? con->height : 100, 0);

	return(XtGeometryYes);
}

/*
 * WWWGeometryManager
 */
static XtGeometryResult WWWGeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply) {
	return(XtGeometryNo);
}

/*
 * WWWSetValues
 */
Boolean WWWSetValues(Widget c, Widget r, Widget n, ArgList args, Cardinal *count) {
	return(False);
}

static void RedrawThumbs(WWWWidget rw) {
	Widget child = rw->www.child;
	Widget clip = rw->www.clip;
	Dimension length, total;
	Position top;

	if (!rw->www.use_scroll) return;

	if (rw->www.horiz_bar != (Widget)NULL) {
		top = -(child->core.x);
		length = clip->core.width;
		total = child->core.width;

		XawScrollbarSetThumb(rw->www.horiz_bar,
			(float)top / (float)total,
			(float)length / (float)total);
	}

	if (rw->www.vert_bar != (Widget)NULL) {
		top = -(child->core.y);
		length = clip->core.height;
		total = child->core.height;

		XawScrollbarSetThumb(rw->www.vert_bar,
			(float)top / (float)total,
			(float)length / (float)total);
	}
}

/*
 * WWWMoveChild
 * Delegates scrolling to the parent Xaw95 Viewport widget when internal
 * scrollbars are disabled, preventing double scrollbars.
 */
void WWWMoveChild(Widget w, int x, int y) {
	WWWWidget rw = (WWWWidget)w;
	Widget parent = XtParent(w);
	register Widget child = rw->www.child;
	register Widget clip = rw->www.clip;

	/*
	 * When embedded inside an outer Viewport (clip -> Viewport), move the WWW
	 * widget inside the Viewport's clip window so the outer Xaw95 Viewport
	 * updates its right/bottom scrollbars instead of internal scrolling.
	 */
	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		int max_x = (int)rw->core.width - (int)parent->core.width;
		int max_y = (int)rw->core.height - (int)parent->core.height;

		if (max_x < 0) max_x = 0;
		if (max_y < 0) max_y = 0;

		if (x < -max_x) x = -max_x;
		if (y < -max_y) y = -max_y;
		if (x > 0) x = 0;
		if (y > 0) y = 0;

		XtMoveWidget(w, (Position)x, (Position)y);
		return;
	}

	if (-x + (int)clip->core.width > (int)child->core.width) {
		x = -(child->core.width - clip->core.width);
	}

	if (-y + (int)clip->core.height > (int)child->core.height) {
		y = -(child->core.height - clip->core.height);
	}

	if (x >= 0) x = 0;
	if (y >= 0) y = 0;

	/* Mmmm hairy cast */
	XtMoveWidget(child, (Position)x, (Position)y);

	RedrawThumbs(rw);
}

static void ScrollUpDownProc(Widget widget, XtPointer closure, XtPointer call_data) {
	WWWWidget rw = (WWWWidget)XtParent(widget);
	register Widget child = rw->www.child;
	int pix = (__intptr_t)call_data;
	Position x, y;

	if (child == NULL) return;  /* no child to scroll. */

	x = child->core.x - ((widget == rw->www.horiz_bar) ? pix : 0);
	y = child->core.y - ((widget == rw->www.vert_bar) ? pix : 0);
	WWWMoveChild((Widget)rw, (int)x, (int)y);
}

static void ThumbProc(Widget widget, XtPointer closure, XtPointer call_data) {
	WWWWidget rw = (WWWWidget)XtParent(widget);
	register Widget child = rw->www.child;
	Position x, y;
	float *percent = (float *)call_data;

	if (child == NULL) return;  /* no child to scroll. */

	if (widget == rw->www.horiz_bar) x = -(int)(*percent * child->core.width);
	else x = child->core.x;

	if (widget == rw->www.vert_bar) y = -(int)(*percent * child->core.height);
	else y = child->core.y;

	WWWMoveChild((Widget)rw, (int)x, (int)y);
}

/*
 * WWWInitialize
 */
static void WWWInitialize(Widget r, Widget n, ArgList args, Cardinal *count) {
	WWWWidget new = (WWWWidget)n;

	/*
	 * Avoid zero.
	 */
	if (new->core.width < 5) new->core.width = 5;
	if (new->core.height < 5) new->core.height = 5;

	new->www.use_scroll = false;

	new->www.resize_closure = NULL;
	new->www.resize_callback = NULL;
	new->www.motion_callback = NULL;
	new->www.motion_closure = NULL;
	new->www.select_callback = NULL;
	new->www.select_closure = NULL;
	new->www.expose_callback = NULL;
	new->www.expose_closure = NULL;

	/*
	 * Keep internal scrollbars unmapped so the enclosing Xaw95 Viewport
	 * exclusively owns and renders the scrollbars on the right/bottom.
	 */
	new->www.vert_bar = XtVaCreateWidget("vert_bar",
		scrollbarWidgetClass, n,
		XtNmappedWhenManaged, False,
		NULL);
	XtManageChild(new->www.vert_bar);

	XtAddCallback(new->www.vert_bar, XtNscrollProc,
		ScrollUpDownProc, (XtPointer)new);
	XtAddCallback(new->www.vert_bar, XtNjumpProc,
		ThumbProc, (XtPointer)new);

	new->www.horiz_bar = XtVaCreateWidget("horiz_bar",
		scrollbarWidgetClass, n,
		XtNmappedWhenManaged, False,
		NULL);
	XtManageChild(new->www.horiz_bar);

	XtAddCallback(new->www.horiz_bar, XtNscrollProc,
		ScrollUpDownProc, (XtPointer)new);
	XtAddCallback(new->www.horiz_bar, XtNjumpProc,
		ThumbProc, (XtPointer)new);

	new->www.clip = XtVaCreateWidget("clip",
		geomWidgetClass, n,
		XtNwidth, new->core.width,
		XtNheight, new->core.height,
		XtNborderWidth, 0,
		NULL);
	XtManageChild(new->www.clip);

	new->www.child = XtVaCreateWidget("child",
		geomWidgetClass, new->www.clip,
		XtNwidth, new->www.clip->core.width,
		XtNheight, new->www.clip->core.height,
		XtNborderWidth, 0,
		NULL);
	XtManageChild(new->www.child);

	XtAugmentTranslations(new->www.child,
		XtParseTranslationTable(defaultTranslations));
}

/*
 * FixDim
 */
static Dimension FixDim(unsigned int d) {
	if (d > 0x7ffd) return(0x7ffd);
	return((Dimension)d);
}

/*
 * WWWGetDrawWidget
 */
Widget WWWGetDrawWidget(Widget w) {
	WWWWidget rw = (WWWWidget)w;
	return(rw->www.child);
}

/*
 * WWWGetSize
 */
int WWWGetDrawSize(Widget w, unsigned int *width, unsigned int *height) {
	WWWWidget rw = (WWWWidget)w;

	*width = (unsigned int)rw->www.child->core.width;
	*height = (unsigned int)rw->www.child->core.height;

	return(0);
}

/*
 * WWWSetDrawSize
 * Expands clip, child, and the WWW widget itself to the full document size at (0,0)
 * while suppressing WWWResize so layout updates do not trigger a redraw loop.
 */
int WWWSetDrawSize(Widget w, unsigned int width, unsigned int height) {
	WWWWidget rw = (WWWWidget)w;
	Widget parent = XtParent(w);
	Dimension dw = FixDim(width);
	Dimension dh = FixDim(height);
	Dimension reply_w, reply_h;
	XtGeometryResult res;

	if (dw < 5) dw = 5;
	if (dh < 5) dh = 5;

	/*
	 * Ensure minimum dimensions match visible Viewport clip so no blank
	 * gutter appears when content is shorter/narrower than the viewport.
	 */
	if (parent != NULL && XtParent(parent) != NULL) {
		if (dw < parent->core.width) dw = parent->core.width;
		if (dh < parent->core.height) dh = parent->core.height;
	}

	in_draw_size = True;

	XtSetMappedWhenManaged(rw->www.vert_bar, False);
	XtSetMappedWhenManaged(rw->www.horiz_bar, False);

	XtConfigureWidget(rw->www.clip, 0, 0, dw, dh, 0);
	XtConfigureWidget(rw->www.child, 0, 0, dw, dh, 0);

	if (rw->core.width != dw || rw->core.height != dh) {
		res = XtMakeResizeRequest(w, dw, dh, &reply_w, &reply_h);
		if (res == XtGeometryAlmost) {
			XtMakeResizeRequest(w, reply_w, reply_h, NULL, NULL);
		}
	}

	in_draw_size = False;

	return(0);
}

/*
 * WWWSetScrollBar
 * Always keep internal scrollbars disabled in favor of the outer Xaw95 Viewport.
 */
void WWWSetScrollBar(Widget w, bool use_scroll) {
	WWWWidget rw = (WWWWidget)w;

	(void)use_scroll;
	rw->www.use_scroll = false;

	XtSetMappedWhenManaged(rw->www.vert_bar, False);
	XtSetMappedWhenManaged(rw->www.horiz_bar, False);
}

/*
 * WWWSetResizeCallback
 */
void WWWSetResizeCallback(Widget w, void (*callback)(), void *closure) {
	WWWWidget rw = (WWWWidget)w;

	rw->www.resize_callback = callback;
	rw->www.resize_closure = closure;
}

/*
 * WWWSetMotionCallback
 */
void WWWSetMotionCallback(Widget w, void (*callback)(), void *closure) {
	WWWWidget rw = (WWWWidget)w;

	rw->www.motion_callback = callback;
	rw->www.motion_closure = closure;
}

/*
 * WWWSetSelectCallback
 */
void WWWSetSelectCallback(Widget w, void (*callback)(), void *closure) {
	WWWWidget rw = (WWWWidget)w;

	rw->www.select_callback = callback;
	rw->www.select_closure = closure;
}

/*
 * WWWSetExposeCallback
 */
void WWWSetExposeCallback(Widget w, void (*callback)(), void *closure) {
	WWWWidget rw = (WWWWidget)w;

	rw->www.expose_callback = callback;
	rw->www.expose_closure = closure;
}

/*
 * WWWExposeAction
 */
static void WWWExposeAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

	if (rw->www.expose_callback != NULL) {
		(rw->www.expose_callback) (w, rw->www.expose_closure,
			xe->xexpose.x, xe->xexpose.y,
			xe->xexpose.width, xe->xexpose.height);
	}
}

/*
 * WWWSelectAction
 */
static void WWWSelectAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

	if (rw->www.select_callback != NULL) {
		(rw->www.select_callback) (w, rw->www.select_closure,
			xe->xbutton.x, xe->xbutton.y,
			xe->xbutton.button);
	}
}

/*
 * WWWMotionAction
 */
static void WWWMotionAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));

	if (rw->www.motion_callback != NULL) {
		(rw->www.motion_callback) (w, rw->www.motion_closure,
			xe->xmotion.x, xe->xmotion.y);
	}
}

/*
 * WWWGetScrollPosition
 */
void WWWGetScrollPosition(Widget w, int *x, int *y) {
	WWWWidget rw = (WWWWidget)w;
	Widget parent = XtParent(w);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		*x = (int)(w->core.x);
		*y = (int)(w->core.y);
		return;
	}

	*x = (int)(rw->www.child->core.x);
	*y = (int)(rw->www.child->core.y);
}

/*
 * WWWPageUpAction
 */
static void WWWPageUpAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x, rw->core.y + parent->core.height / 2);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x, rw->www.child->core.y + rw->www.clip->core.height / 2);
}

/*
 * WWWPageDownAction
 */
static void WWWPageDownAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x, rw->core.y - parent->core.height / 2);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x, rw->www.child->core.y - rw->www.clip->core.height / 2);
}

/*
 * WWWPageLeftAction
 */
static void WWWPageLeftAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x + parent->core.width / 2, rw->core.y);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x + rw->www.clip->core.width / 2, rw->www.child->core.y);
}

/*
 * WWWPageRightAction
 */
static void WWWPageRightAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x - parent->core.width / 2, rw->core.y);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x - rw->www.clip->core.width / 2, rw->www.child->core.y);
}

/*
 * WWWScrollUpAction
 */
static void WWWScrollUpAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x, rw->core.y + 10);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x, rw->www.child->core.y + 10);
}

/*
 * WWWScrollDownAction
 */
static void WWWScrollDownAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x, rw->core.y - 10);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x, rw->www.child->core.y - 10);
}

/*
 * WWWScrollLeftAction
 */
static void WWWScrollLeftAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x + 10, rw->core.y);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x + 10, rw->www.child->core.y);
}

/*
 * WWWScrollRightAction
 */
static void WWWScrollRightAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	WWWWidget rw = (WWWWidget)XtParent(XtParent(w));
	Widget parent = XtParent((Widget)rw);

	if (!rw->www.use_scroll && parent != NULL && XtParent(parent) != NULL) {
		WWWMoveChild((Widget)rw, rw->core.x - 10, rw->core.y);
		return;
	}

	WWWMoveChild((Widget)rw, rw->www.child->core.x - 10, rw->www.child->core.y);
}
