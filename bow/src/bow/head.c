#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "common.h"
#include <athena.h>
#include "BowP.h"

static void Quit _ArgProto((Widget, XtPointer, XtPointer));
static void Back _ArgProto((Widget, XtPointer, XtPointer));
static void Reload _ArgProto((Widget, XtPointer, XtPointer));
static void Cancel _ArgProto((Widget, XtPointer, XtPointer));
static void Home _ArgProto((Widget, XtPointer, XtPointer));
static void Dup _ArgProto((Widget, XtPointer, XtPointer));
static void Help _ArgProto((Widget, XtPointer, XtPointer));
static void AddMark _ArgProto((Widget, XtPointer, XtPointer));
static void ViewMark _ArgProto((Widget, XtPointer, XtPointer));
static void Bookmark _ArgProto((Widget, XtPointer, XtPointer));
static void Source _ArgProto((Widget, XtPointer, XtPointer));
static void Save _ArgProto((Widget, XtPointer, XtPointer));
static void Go _ArgProto((Widget, XtPointer, XtPointer));

static void OOpen _ArgProto((Widget, XtPointer, XtPointer));
static void DOpen _ArgProto((Widget, XtPointer, XtPointer));
static void Open _ArgProto((Widget, XtPointer, XtPointer));

static void OFind _ArgProto((Widget, XtPointer, XtPointer));
static void DFind _ArgProto((Widget, XtPointer, XtPointer));
static void Find _ArgProto((Widget, XtPointer, XtPointer));

/*
 * HeadCreate
 *
 * Setup bow in a widget.
 */
static void CreateWidgets(BowContext wc) {
	Widget paned, box, form;
	int tool_row = 0;

	wc->toplevel = wc->athena->topLevel;
	MwHighlightInit(wc->athena->topLevel);

/*
 * Split `Main window` into grid layout, Menu,
 * Button & Url field, WWW content, and Status pane.
 *
 * If `webview`, no menus, show toolbar `buttons` only if set.
 */
	if (wc->athena->webview_set) {
		wc->athena->grid = ats_gridthree_set(wc->toplevel, (wc->athena->web->showtoolbar ? 30 : 0), 20);
	} else {
		tool_row = 1;
		wc->athena->toolheight = 28;
		ats_menubar_set(wc->athena, 2);

		static menuitem_t menu_items[] = {
			{110, "Open", (_menu_cb)Open, "O", NULL},
			{111, "Save", (_menu_cb)Save, "S", NULL},
			{__ATS_SEPERATOR__},
			{112, "Quit", (_menu_cb)Quit, "Q", NULL},
		};

		static menuitem_t menu_items_two[] = {
			{113, "Bookmark", (_menu_cb)Bookmark, "M", NULL},
			{114, "Find", (_menu_cb)Find, "F", NULL},
			{115, "Source", (_menu_cb)Source, "V", NULL},
			{__ATS_SEPERATOR__},
			{116, "Help", (_menu_cb)Help, "H", NULL},
		};

		wc->athena->bar_info->override = true;
		wc->athena->bar_info->override_data = wc;
		if (!ats_font_set(wc->athena, helvetica)
			|| !ats_menu_set(wc->athena, 0, menu_items, 4, 1, "File")
			|| !ats_menu_set(wc->athena, 1, menu_items_two, 5, 2, "Tools")) {
			fprintf(stderr, "Error: can't create `font` or `menu` items!\n");
			return;
		}
	}
	paned = wc->athena->grid;

/*
 * Button pane(s)
 */
	if (!wc->athena->webview_set || wc->athena->web->showtoolbar) {
		box = ats_navigation_set(paned, tool_row);
		wc->athena->app->app_data = (void *)wc;
		ats_toolbar_set(wc->athena, box, Home, "home.xpm", "Home", false);
		ats_toolbar_set(wc->athena, box, Back, "back.xpm", "Back", false);
		//toolcmd = ats_toolbar_set(wc->athena, form, cb_forward, "forward.xpm", "Forward", false);
		ats_toolbar_set(wc->athena, box, Reload, "reload.xpm", "Reload", false);
		form = ats_toolbar_set(wc->athena, box, Cancel, "cancel.xpm", "Cancel", false);
		if (!wc->athena->webview_set) {
			ats_toolbar_set(wc->athena, box, Open, "fld_open.xpm", "Open", false);
			form = ats_toolbar_set(wc->athena, box, Save, "save.xpm", "Save", false);
		}

		/*
		 * URL pane
		 */
		wc->url = ats_textfield_set(box, "", 0, 1, (wc->athena->width - (40 * (wc->athena->numtools + 1))), 26);
		XtVaSetValues(wc->url, XtNborderColor, 0xffffff, NULL);
		ats_alignfield(wc->url, form, false);
		wc->athena->webview_set = true;
		wc->athena->web->width = wc->athena->width;
		wc->athena->web->height = wc->athena->height;
		wc->athena->web->priv.inspector_window = wc->url;

		form = ats_toolbar_set(wc->athena, box, Go, "preview.xpm", "Go", false);
		ats_alignfield(form, wc->url, false);
		XtOverrideTranslations(wc->url, XtParseTranslationTable("<Key>Return: ReturnAction()"));
	}

/*
 * WWW widget
 */
	box = ats_mainarea_set(wc->athena, paned, tool_row + 1);

/*
 * Message `statusline` pane.
 */
	wc->message = ats_status_set(paned, "", tool_row + 2, 500, 10);
	ats_alignfield(wc->message, box, false);

	paned = ats_viewport_set(box, 0, false, false);
	wc->tstack = StackCreateToplevel(wc, paned);
	XtRealizeWidget(wc->toplevel);
	wc->athena->win = XtWindow(wc->toplevel);
	wc->athena->app->app_data = wc->toplevel;
	wc->athena->app->wnd = paned;
	wc->athena->app->name = wc->athena->title;
	wc->athena->web->priv.ats = wc->athena;
	wc->athena->web->priv.context = wc;
	wc->athena->wmDeleteMessage = ats_display_wm(wc->cres->dpy, wc->athena->win, wc->athena->use_icon, NULL, NULL);
	XtOverrideTranslations(wc->toplevel, XtParseTranslationTable("<Message>WM_PROTOCOLS: DeleteAction()"));
}

/*
 * HeadDestroy
 */
void HeadDestroy(BowContext wc) {
	StackDestroy(wc->tstack);
	GListRemoveItem(wc->cres->heads, wc);
	ats_t *ui = wc->athena;
	ats_cancel(ui->topLevel);
	MPDestroy(wc->mp);
	BowRemoveReference(wc->cres);
	ui->exited = 1;
}

/*
 * ClearDialogValue
 */
static void ClearDialogValue(Widget w, XtPointer cldata, XtPointer cbdata) {
	if ((w = XtParent(w)) != NULL)
		XtVaSetValues(w, XtNvalue, "", NULL);
}

/*
 * CreateDialog
 */
Widget CreateDialog(Widget p, char *name, void (*ofunc)(), void (*dfunc)(),
	void (*rfunc)(), XtPointer closure) {
	Widget w, dw;

	w = ats_modalwindow_set(p, name);
	dw = ats_dialogwindow_set(w, "dialog", ATHENA_COLOR, name, "");
	XawDialogAddButton(dw, "ok", ofunc, closure);
	XawDialogAddButton(dw, "clear", ClearDialogValue, closure);
	XawDialogAddButton(dw, "dismiss", dfunc, closure);

	XtRealizeWidget(w);
	ats_display_top(w, p, w, false);

	return(w);
}

/*
 * GetDialogWidget
 */
Widget GetDialogWidget(Widget w) {
	return(XtNameToWidget(w, "dialog"));
}

/*
 * OOpen
 */
static void OOpen(Widget ww, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *url;

	if ((url = XawDialogGetValueString(GetDialogWidget(wc->openpop))) == NULL) return;

	StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
	XtPopdown(wc->openpop);
}

/*
 * DOpen
 */
static void DOpen(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;

	ats_display_top(w, wc->toplevel, wc->openpop, true);
	XtPopdown(wc->openpop);
}

/*
 * Open
 */
static void Open(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;

	if (wc->openpop == NULL) {
		wc->openpop = CreateDialog(wc->toplevel, "Enter URL",
			OOpen, DOpen, OOpen, (XtPointer)wc);
	}
	XtPopup(wc->openpop, XtGrabNone);
}

/*
 * Back
 */
static void Back(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	StackBack(wc->tstack);
}

/*
 * Reload
 */
static void Reload(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	StackReload(wc->tstack);
}

/*
 * Cancel
 */
static void Cancel(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	StackCancel(wc->tstack);
}

/*
 * Quit
 */
static void Quit(Widget w, XtPointer cldata, XtPointer cbdata) {
	HeadDestroy((BowContext)cldata);
}

/*
 * Source
 */
static void Source(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;

	if (wc->cres->plainhooks == NULL) XtVaSetValues(w, XtNstate, False, NULL);
	else {
		if (cbdata) StackSetRender(wc->tstack, wc->cres->plainhooks);
		else StackSetRender(wc->tstack, NULL);
		StackRedraw(wc->tstack);
	}
}

/*
 * Save
 */
static void Save(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *url;
	BowRequest *wr;

	if ((url = StackGetCurrentURL(wc->tstack)) == NULL) return;

	if ((wr = RequestCreate(wc->cres, url, NULL)) == NULL) {
		TextFieldSetString(wc->message, "Invalid URL.");
		return;
	}
	DownloadOpen(wc->cres, wr);
}

/*
 * Home
 */
static void Home(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	StackHome(wc->tstack);
}

/*
 * Help
 */
static void Help(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *url;

	if ((url = ResourceGetString(wc->cres, "bow.helpURL")) != NULL) {
		StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));
	}
}

/*
 * Dup
 */
static void Dup(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *url;
	BowRequest *wr;

	/* grab URL from to-be-cloned window */
	if ((url = ResourceGetString(wc->cres, "bow.cloneHome")) == NULL) {
		url = StackGetCurrentURL(wc->tstack);
	}

	if (url != NULL) wr = RequestCreate(wc->cres, url, NULL);
	else wr = NULL;

	HeadCreate(wc->cres, NULL, wr);
}

static void Go(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowResources globalcres = ((BowContext)cldata)->cres;
	BowContext c;
	char *url;

	for (c = (BowContext)GListGetHead(globalcres->heads); c != NULL;
		c = (BowContext)GListGetNext(globalcres->heads)) {
		if (c->url == w) {
			url = TextFieldGetString(c->url);
			if (url == NULL || url[0] == '\0') {
				TextFieldSetString(c->url, url);
				break;
			} else {
				StackOpen(c->tstack, RequestCreate(globalcres, url, NULL));
			}
			break;
		}
	}
}

/*
 * AddMark
 */
void AddMark(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *title;
	char *url;
	BowRender wn;

	if (wc->cres->bc == NULL) return;

	wn = StackToRender(wc->tstack);
	if ((url = RenderQuery(wn, "url")) == NULL) return;
	if ((title = RenderQuery(wn, "title")) == NULL) title = url;

	BookmarkAdd(wc->cres->bc, title, url);
}

/*
 * ViewMark
 */
void ViewMark(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	MemPool mp;
	char *filename;
	char *url;
	const char *fformat = "file:%s";

	mp = MPCreate();

	if ((filename = ResourceGetFilename(wc->cres,
		mp, "bookmark.filename")) == NULL) {
		return;
	}

	url = (char *)MPGet(mp, strlen(fformat) + strlen(filename) + 1);
	sprintf(url, fformat, filename);
	StackOpen(wc->tstack, RequestCreate(wc->cres, url, NULL));

	MPDestroy(mp);
}

/*
 * Bookmark
 */
static void Bookmark(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;

	wc->cres->bmcontext = wc;
	if (wc->cres->bc != NULL)
		BookmarkShow(wc->cres->bc);
}

/*
 * OFind
 */
static void OFind(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	char *str;

	if ((str = XawDialogGetValueString(GetDialogWidget(wc->findpop))) == NULL) {
		return;
	}

	RenderSearch(StackToRender(wc->tstack), str, 0);

	XtPopdown(wc->findpop);
}

/*
 * DFind
 */
static void DFind(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;
	ats_display_top(w, wc->toplevel, wc->findpop, true);
	XtPopdown(wc->findpop);
}

/*
 * Find
 */
static void Find(Widget w, XtPointer cldata, XtPointer cbdata) {
	BowContext wc = (BowContext)cldata;

	if (wc->findpop == NULL) {
		wc->findpop = CreateDialog(wc->toplevel, "Search",
			OFind, DFind, OFind, (XtPointer)wc);
	}
	XtPopup(wc->findpop, XtGrabNone);
}

/*
 * HeadCreate
 *
 * Frontend that sorts out which URL to use for a new head and then
 * creates a new head.
 */
void HeadCreate(BowResources cres, BowRequest *first, BowRequest *lastresort) {
	BowContext wc;
	char *buffer;
	char *url;
	int count;
	MemPool mp;
	BowRequest *wr = NULL;
	char base_url[255];

	/*
	 * Default base URL; this allows filenames to be used on the
	 * command line
	 */
	strcpy(base_url, "file:");
	getcwd(base_url + 5, sizeof(base_url) - 5);
	strcat(base_url, "/");

	BowAddReference(cres);

	mp = MPCreate();
	wc = (BowContext)MPCGet(mp, sizeof(struct BowContextP));
	wc->athena = cres->ats;
	wc->mp = mp;
	wc->cres = cres;

	GListAddHead(cres->heads, wc);

	CreateWidgets(wc);

	/*
	 * If an override URL is supplied then try to use that.
	 */
	if (first != NULL) wr = first;

	/*
	 * Look inside the cutbuffer for a valid URL.
	 */
	if (wr == NULL) {
		buffer = XFetchBytes(cres->dpy, &count);
		if (count > 0) {
			mp = MPCreate();
			url = (char *)MPCGet(mp, count + 1);
			memcpy(url, buffer, count);
			url[count] = '\0';

			XFree(buffer);

			if ((wr = RequestCreate(wc->cres, url, base_url)) != NULL) {
				XStoreBytes(cres->dpy, "", 0);
			}

			MPDestroy(mp);
		}
	}

	/*
	 * Look for the caller-supplied last resort.
	 */
	if (wr == NULL && lastresort != NULL) wr = lastresort;

	/*
	 * Check the standard environment variable.
	 */
	if (wr == NULL) {
		if ((url = getenv("WWW_HOME")) != NULL) {
			if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL) {
				fprintf(stderr, "WWW_HOME (%s) is invalid.\n", url);
			}
		}
	}

	/*
	 * Check the bow resource variable.
	 */
	if (wr == NULL) {
		if ((url = ResourceGetString(wc->cres, "bow.homeURL")) != NULL) {
			if ((wr = RequestCreate(wc->cres, url, base_url)) == NULL) {
				fprintf(stderr, "bow.homeURL (%s) is invalid.\n", url);
			}
		}
	}

	/*
	 * This better work...
	 */
	if (wr == NULL) {
		wr = RequestCreate(wc->cres, "file:/", NULL);
	}

	myassert(wr != NULL, "ERROR: No valud URLs found for new head.\n");
	StackOpen(wc->tstack, wr);
}

/*
 * HeadPrintMessage
 */
void HeadPrintMessage(BowContext wc, char *message) {
	if (message == NULL) message = "";
	TextFieldSetString(wc->message, message);
}

/*
 * HeadPrintURL
 */
void HeadPrintURL(BowContext wc, char *url) {
	if (url == NULL) url = "";
	if (wc->url)
		TextFieldSetString(wc->url, url);
}
