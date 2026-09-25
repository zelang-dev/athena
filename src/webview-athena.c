#if defined(__linux__)
#include "ats_internal.h"

struct hist {
	char *url;
	int top_row;
	struct hist *next;
};

/* General functions */
static void fwfree(struct hist *h) {
	if (h) {
		fwfree(h->next);
		free(h->url);
		free(h);
	}
}

static void adjust_scrollbars(ats_t *ui, Widget w) {
	int tr, tc, tw, th;
	Dimension ww, wh;
	float pos, size;

	XtVaGetValues(w,
		XtNtopRow, &tr,
		XtNtopCol, &tc,
		XtNtotalWidth, &tw,
		XtNtotalHeight, &th,
		XtNwidth, &ww,
		XtNheight, &wh,
		NULL);
	pos = (float)tr / (float)th;
	size = (float)wh / (float)th;
	XawScrollbarSetThumb((ui == NULL ? main_athena_info->web->priv.scroller : ui->web->priv.scroller),
		pos, size);
	pos = (float)tc / (float)tw;
	size = (float)ww / (float)tw;
}

static void new_url(Widget w, ats_t *ui, char *u) {
	struct hist *h = MwMalloc(sizeof *h);
	char *v;
	int r;
	XtVaGetValues(ui->web->priv.webview,
		XtNurl, &v,
		XtNtopRow, &r,
		NULL);
	if (v) v = MwStrdup(v);
	else v = MwStrdup(u);
	XtVaSetValues(ui->web->priv.webview,
		XtNurl, u,
		XtNtopRow, 0,
		NULL);
	XtVaGetValues(ui->web->priv.webview, XtNurl, &u, NULL);
	if (strcmp(u, v)) {
		h->url = v;
		h->top_row = r;
		h->next = ui->web->priv.backhist;
		ui->web->priv.backhist = h;
		fwfree(ui->web->priv.forwhist);
		ui->web->priv.forwhist = NULL;
	} else {
		MwFree(v);
		MwFree(h);
	}
	adjust_scrollbars(ui, w);
}

static void scroll_updown(Widget w, ats_t *ui, float amount) {
	Dimension height;
	int top_row, th;

	XtVaGetValues((ui == NULL ? main_athena_info->web->priv.webview : ui->web->priv.webview),
		XtNtotalHeight, &th,
		XtNheight, &height,
		XtNtopRow, &top_row,
		NULL);
	top_row += amount * height;
	if (top_row > th) top_row = th;
	if (top_row < 0) top_row = 0;
	XtVaSetValues((ui == NULL ? main_athena_info->web->priv.webview : ui->web->priv.webview),
		XtNtopRow, top_row,
		NULL);
	adjust_scrollbars(ui, (ui == NULL ? main_athena_info->web->priv.webview : ui->web->priv.webview));
}

static void web_page_down(Widget w) {
	scroll_updown(w, NULL, 0.9);
}

static void web_page_up(Widget w) {
	scroll_updown(w, NULL, -0.9);
}

static void web_scroll_down(Widget w) {
	scroll_updown(w, NULL, 0.1);
}

static void web_scroll_up(Widget w) {
	scroll_updown(w, NULL, -0.1);
}

static void quit(Widget w, XEvent *event, String *params, Cardinal *n) {
}

static void page_down(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_page_down(w);
}

static void page_up(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_page_up(w);
}

static void scroll_down(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_scroll_down(w);
}

static void scroll_up(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_scroll_up(w);
}

static void web_scroll_leftright(Widget w, float amount) {
	Dimension width;
	int top_col, pw;

	XtVaGetValues(w,
		XtNtotalWidth, &pw,
		XtNwidth, &width,
		XtNtopCol, &top_col,
		NULL);
	top_col += amount * width;
	if (top_col > pw) top_col = pw;
	if (top_col < 0) top_col = 0;
	XtVaSetValues(w,
		XtNtopCol, top_col,
		NULL);
	adjust_scrollbars(NULL, w);
}

static void scroll_left(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_scroll_leftright(w, -0.1);
}

static void scroll_right(Widget w, XEvent *event, String *params, Cardinal *n) {
	web_scroll_leftright(w, 0.1);
}

static void cb_back(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	struct hist *h = ui->web->priv.backhist;
	char *u;
	int r;
	char *v;
	if (h) {
		XtVaGetValues(ui->web->priv.webview,
			XtNurl, &v,
			XtNtopRow, &r,
			NULL);
		v = MwStrdup(v);
		XtVaSetValues(ui->web->priv.webview,
			XtNurl, h->url,
			XtNtopRow, h->top_row,
			NULL);
		XtVaGetValues(ui->web->priv.webview, XtNurl, &u, NULL);
		MwFree(h->url);
		if (strcmp(u, v)) {
			ui->web->priv.backhist = h->next;
			h->url = v;
			h->top_row = r;
			h->next = ui->web->priv.forwhist;
			ui->web->priv.forwhist = h;
		} else {
			MwFree(h);
			MwFree(v);
		}
		adjust_scrollbars(ui, ui->web->priv.webview);
	}
}

static void cb_forward(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	struct hist *h = ui->web->priv.forwhist;
	char *u;
	int r;
	char *v;
	if (h) {
		XtVaGetValues(ui->web->priv.webview,
			XtNurl, &v,
			XtNtopRow, &r,
			NULL);
		v = MwStrdup(v);
		XtVaSetValues(ui->web->priv.webview,
			XtNurl, h->url,
			XtNtopRow, h->top_row,
			NULL);
		XtVaGetValues(ui->web->priv.webview, XtNurl, &u, NULL);
		MwFree(h->url);
		if (strcmp(u, v)) {
			ui->web->priv.forwhist = h->next;
			h->url = v;
			h->top_row = r;
			h->next = ui->web->priv.backhist;
			ui->web->priv.backhist = h;
		} else {
			MwFree(v);
			MwFree(h);
		}
		adjust_scrollbars(ui, ui->web->priv.webview);
	}
}

static void cb_reload(__ATS_WEBVIEW__) {
	char *u;
	XtVaGetValues(self, XtNurl, &u, NULL);
	u = MwStrdup(u);
	XtVaSetValues(self, XtNurl, u, NULL);
	MwFree(u);
	adjust_scrollbars((ats_t *)client, self);
}

static void cb_cancel(__ATS_WEBVIEW__) {
	printf("cb_cancel()\n");
}

static void cb_home(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	new_url(ui->web->priv.webview, ui, (char *)ui->web->url);
}

static void cb_goto(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	String value = TextFieldGetString((Widget)ui->web->userdata);
	if (is_ValidUrl(value))
		new_url(ui->web->priv.webview, ui, value);

	free(value);
}

static void cb_error(__ATS_WEBVIEW__) {
	MwErrorBox(self, "Nothing to see here.");
}

static void cb_click(__ATS_WEBVIEW__) {
	new_url(self, client, (char *)data);
}

static void cb_open(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	static char path[1024];
	char name[1024] = "";
	char *patterns[] = {"All files (*)", NULL};
	char fmt[1024] = "";
	char *extra = NULL;
	int n, ext = 0;
	char b[1024];

	if (path[0] == '\0') strcpy(path, ".");
	n = MwFileselInput(ui->topLevel, path, name, patterns, fmt, extra, ext);
	if (n) {
		MwSnprintf(b, sizeof b, "%s/%s", path, name);
		new_url(self, ui, b);
	}
}

static void cb_save(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	static char path[1024];
	char name[1024] = "";
	char *patterns[] = {"All files (*)", NULL};
	char fmt[1024] = "";
	char *extra = NULL;
	int n, ext = 0;
	char b[1024];
	char *url;

	if (path[0] == '\0') strcpy(path, ".");
	n = MwFileselInput(ui->topLevel, path, name, patterns, fmt, extra, ext);
	XtVaGetValues(ui->web->priv.webview, XtNurl, &url, NULL);
	if (n) {
		MwSnprintf(b, sizeof b, "%s/%s", path, name);
		MwHtmlSave(url, b);
	}
}

static void cb_url(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	char *u = (char *)data;
	if (ui->web->showtoolbar)
		TextFieldSetString((Widget)ui->web->userdata, u);

	adjust_scrollbars(ui, self);
}

static void cb_vscroll_jump(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	float top;
	int top_row, th;

	XtVaGetValues(self, XtNtotalHeight, &th, NULL);
	XtVaGetValues(self, XtNtopOfThumb, &top, NULL);
	top_row = top * th;
	XtVaSetValues(self, XtNtopRow, top_row, NULL);
}

static void cb_vscroll_scroll(__ATS_WEBVIEW__) {
	ats_t *ui = (ats_t *)client;
	int i = (long)data;
	Dimension length, height;
	int top_row, th;
	float amount;

	XtVaGetValues(self, XtNlength, &length, NULL);
	XtVaGetValues(self,
		XtNheight, &height,
		XtNtopRow, &top_row,
		XtNtotalHeight, &th,
		NULL);
	if (i < 0) {
		if ((length / -i) > 15) {
			amount = -0.1;
		} else {
			amount = -0.9;
		}
	} else {
		if ((length / i) > 15) {
			amount = 0.1;
		} else {
			amount = 0.9;
		}
	}
	scroll_updown(self, ui, amount);
}

static Widget add_command(Widget pw, _platform_cb cb, XtPointer closure, char *pm) {
	Widget w;
	Pixmap pm_return;
	Pixel color;
	XtVaGetValues(pw, XtNbackground, &color, NULL);
	w = XtVaCreateManagedWidget("tooltip",
		commandWidgetClass, pw,
		XtNshadowWidth, 0,
		XtNforeground, color,
		NULL);
	pm_return = MwLoadPixmap(XtDisplay(pw), color, pm);
	XtVaSetValues(w, XtNbitmap, pm_return, NULL);
	XtAddCallback(w, XtNcallback, cb, closure);
	return w;
}

static char webview_hishory[10] = {0};

static XtActionsRec web_actions[] = {
	{"page_down", page_down},
	{"page_up", page_up},
	{"scroll_down", scroll_down},
	{"scroll_up", scroll_up},
	{"scroll_left", scroll_left},
	{"scroll_right", scroll_right},
	{"quit", quit},
};

int webview_create(ats_t *ui, webview_t *w) {
	Widget toolcmd;
	char b[1024], *p;

	if (!(p = getenv("HOME")))
		p = "/tmp";

	sprintf(b, "%s/.webview", p);
	mkdir(b, 0700);
	strcat(b, "/cache");
	mkdir(b, 0700);

	if (getenv("PIXPATH") == NULL) {
		sprintf(b, "PIXPATH=%s", DEFAULT_DATAPATH);
		putenv(b);
	}

	ui->use_icon = icon_32x32;
	ui->webview_set = true;
	if (ats_window(ui, w->title, w->width, w->height, false)) {
		XtVaSetValues(ui->topLevel, XtNbackground, 0x808080, NULL);
		XtAppAddActions(ui->app_con, web_actions, XtNumber(web_actions));
		MwHighlightInit(ui->topLevel);
		w->priv.window = ats_windowgrid_set(ui, 30, 30);
		Widget statbar = ats_gridlayout_set(w->priv.window, 0, 2, "100%", "100%");

		ui->statusLine = ats_statusline_set(statbar, w->priv.window, "", 0, 0, w->width - 20);
		XtVaGetValues(ui->statusLine, XtNbackground, &ui->color, NULL);
		if (w->showtoolbar) {
			Widget navbar = ats_gridlayout_set(w->priv.window, 0, 0, "100%", NULL);
			Widget navbox = ats_boxspace_set(navbar, 0, 0);

			ats_toolbar_set(ui, navbox, cb_home, "home.xpm", "Home", true);
			ats_toolbar_set(ui, navbox, cb_back, "back.xpm", "Back", true);
			toolcmd = ats_toolbar_set(ui, navbox, cb_forward, "forward.xpm", "Forward", true);
			//ats_toolbar_set(ui, navbox, cb_reload, "reload.xpm", "Reload");
			//ats_toolbar_set(ui, navbox, cb_cancel, "cancel.xpm", "Cancel");
			//ats_toolbar_set(ui, navbox, cb_open, "fld_open.xpm", "Open");
			//ats_toolbar_set(ui, navbox, cb_save, "save.xpm", "Save");

			XtVaGetValues(navbox, XtNbackground, &ui->color, NULL);
			int numtools = 4;
			w->priv.inspector_window = ats_gridbar_set(navbox, (ui->width - (38 * numtools)), 28, "95%");
			XtVaSetValues(w->priv.inspector_window, XtNborderColor, ui->color, NULL);

			Widget addressfield = ats_field_set(w->priv.inspector_window, toolcmd, "https://",
				0, 0, 100, field_url, NULL);
			XtAddCallback(addressfield, XtNactivateCallback, cb_goto, ui);
			w->userdata = (void *)addressfield;

			toolcmd = ats_toolbar_set(ui, navbox, cb_goto, "preview.xpm", "Go", true);
			ui->user_data = (void *)toolcmd;
			ats_alignfield(toolcmd, addressfield, false);
		}

		Widget viewport = XtVaCreateManagedWidget("viewport",
			mwRudegridWidgetClass, w->priv.window,
			XtNgridy, 1,
			XtNbackground, ui->color,
			XtNxLayout, "50% 100 50% 17 17",
			NULL);

		w->priv.webview = XtVaCreateManagedWidget("html",
			mwHtmlWidgetClass, viewport,
			XtNtopCol, -10,
			XtNstatus, ui->statusLine,
			XtNgridWidth, 4,
			XtNborderWidth, 0,
			XtNdelay, 10,
			NULL);
		XtAddCallback(w->priv.webview, XtNcallback, cb_click, ui);
		XtAddCallback(w->priv.webview, XtNchangeUrl, cb_url, ui);
		XtVaSetValues(w->priv.webview, XtNurl, w->url, NULL);

		w->priv.scroller = (void *)XtVaCreateManagedWidget("vscroll",
			scrollbarWidgetClass, viewport,
			XtNgridx, 4,
			XtNorientation, XtorientVertical,
			NULL);

		main_athena_info->web->priv.window = w->priv.window;
		main_athena_info->web->priv.webview = w->priv.webview;
		main_athena_info->web->priv.scroller = w->priv.scroller;
		XtAddCallback((Widget)w->priv.scroller, XtNjumpProc, cb_vscroll_jump, ui);
		XtAddCallback((Widget)w->priv.scroller, XtNscrollProc, cb_vscroll_scroll, ui);

		XtVaSetValues(ui->statusLine, XtNjustify, XtJustifyLeft, XtNbackground, ui->color, NULL);
		XtVaSetValues(statbar, XtNbackground, ui->color, NULL);
		ui->app->app_data = (void *)w->priv.window;
		ui->app->name = w->title;
		ui->app->wnd = ui->topLevel;
		ui->app->ats = ui;
		ui->wnd = ui->topLevel;
		w->priv.ats = ui;
		w->priv.ats->win = ui->win;
		w->priv.ats->dpy = ui->dpy;
		return 1;
	}

	return 0;
}

int webview_loop(webview_t *w, int blocking) {
	blocking = 1;
	if (w->priv.ats) {
		XFlush(w->priv.ats->dpy);
		for (;;) {
			XtAppNextEvent(w->priv.ats->app_con, &w->priv.ats->xev);
			XtDispatchEvent(&w->priv.ats->xev);
			if (w->priv.ats->xev.xclient.type == ClientMessage
				&& w->priv.ats->xev.xclient.data.l[0] == w->priv.ats->wmDeleteMessage) {
				break;
			} else if (w->priv.ats->xev.type == ConfigureNotify) {
				XConfigureEvent xce = w->priv.ats->xev.xconfigure;
				/* This event type is generated for a variety of
				   happenings, so check whether the window has been
				   resized. */
				if (xce.width != w->width) {
					int numtools = 4;
					w->width = xce.width;
					if (w->priv.inspector_window)
						XtResizeWidget((Widget)w->priv.inspector_window, (w->width - (38 * numtools)), 28, 1);

					//if (w->priv.ats->user_data)
					//	XtMoveWidget((Widget)w->priv.ats->user_data, w->width - 38, 0);
				}
			}
		}
		XtUnrealizeWidget(w->priv.ats->topLevel);
		blocking = 0;
	}

	return blocking;
}

FORCEINLINE void webview_set_title(webview_t *w, const char *title) {
}

FORCEINLINE void webview_set_fullscreen(webview_t *w, int fullscreen) {
}

FORCEINLINE void webview_set_color(webview_t *w, uint8_t r, uint8_t g,
	uint8_t b, uint8_t a) {
}

FORCEINLINE void webview_dialog(webview_t *w,
	enum webview_dialog_type dlgtype, int flags,
	const char *title, const char *arg,
	char *result, size_t resultsz) {
}

FORCEINLINE int webview_eval(webview_t *w, const char *js) {
	return 0;
}

FORCEINLINE void webview_dispatch(webview_t *w, webview_dispatch_fn fn,
	void *arg) {
}

FORCEINLINE void webview_exit(webview_t *w) {
	if (w->priv.ats) {
		XtDestroyApplicationContext(w->priv.ats->app_con);
		fwfree(w->priv.forwhist);
	}
}
#endif