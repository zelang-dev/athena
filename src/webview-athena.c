#if defined(__linux__)
#include <athena.h>

struct hist {
	char *url;
	int top_row;
	struct hist *next;
};

int webview_create(ats_t *ui, webview_t *w) {
	Widget toolcmd;
	ui->use_icon = icon_32x32;
	ui->webview_set = true;
	if (ats_window(ui, w->title, w->width, w->height, false)) {
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
	bow_set_title(w->priv.web, title);
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

FORCEINLINE int ats_webview(ats_t *ui, const char *title, const char *url,
	int width, int height, bool showtoolbar) {
	if (tls_init() != 0) {
		webview_debug("tls_init failed");
		return 0;
	}

	ui->web->url = url;
	ui->web->title = title;
	ui->web->width = width;
	ui->web->height = height;
	ui->web->resizable = 1;
	ui->web->showtoolbar = showtoolbar;
	ui->web->debug = 1;
	return webview_create(ui, ui->web);
}

FORCEINLINE void ats_webactive(ats_t *ui) {
	webview_loop(ui->web, 1);
}

FORCEINLINE void ats_webdestroy(ats_t *ui) {
	webview_exit(ui->web);
}

WEBVIEW_API int webview_run(const char *title, const char *url, int width, int height) {
	ats_t ui = {0};
	ats_webview(&ui, title, url, width, height, true);
	ats_webactive(&ui);
	ats_webdestroy(&ui);
	return 0;
}

WEBVIEW_API void webview_debug(const char *format, ...) {
#ifdef USE_DEBUG
	char buf[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	webview_print_log(buf);
	va_end(ap);
#endif
}

static int webview_js_encode(const char *s, char *esc, size_t n) {
	int r = 1; /* At least one byte for trailing zero */
	for (; *s; s++) {
		const unsigned char c = *s;
		if (c >= 0x20 && c < 0x80 && strchr("<>\\'\"", c) == NULL) {
			if (n > 0) {
				*esc++ = c;
				n--;
			}
			r++;
		} else {
			if (n > 0) {
				snprintf(esc, n, "\\x%02x", (int)c);
				esc += 4;
				n -= 4;
			}
			r += 4;
		}
	}
	return r;
}

WEBVIEW_API int webview_inject_css(webview_t *w, const char *css) {
	int n = webview_js_encode(css, NULL, 0);
	char *esc = (char *)calloc(1, sizeof(CSS_INJECT_FUNCTION) + n + 4);
	if (esc == NULL) {
		return -1;
	}
	char *js = (char *)calloc(1, n);
	webview_js_encode(css, js, n);
	snprintf(esc, sizeof(CSS_INJECT_FUNCTION) + n + 4, "%s(\"%s\")",
		CSS_INJECT_FUNCTION, js);
	int r = webview_eval(w, esc);
	free(js);
	free(esc);
	return r;
}

FORCEINLINE void webview_print_log(const char *s) {
#ifdef USE_DEBUG
	fprintf(stderr, "%s\n", s);
#endif
}
#endif