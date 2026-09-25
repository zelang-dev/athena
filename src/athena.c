#include <Athena.h>
#include <TextField.h>
#include <Gridbox.h>
#include "libsmallregex.h"

static ats_t *main_athena_info = NULL;
static volatile bool main_athena_shutdown = false;

#if !defined(ATHENA_ICON)
#	define ATHENA_ICON "../share/pixmaps/athena.xpm"
#endif

#define INFO_ICON "../share/pixmaps/info.xpm"
#define WHY_ICON "../share/pixmaps/why.xpm"
#define SECURITY_ICON "../share/pixmaps/security.xpm"
#define FILEOPEN_ICON "../share/pixmaps/fld_open.xpm"
#define FILEEDIT_ICON "../share/pixmaps/editor.xpm"
#define NONE_ICON "../share/pixmaps/none.xpm"

// load window icons
#include ATHENA_ICON
#include INFO_ICON
#include WHY_ICON
#include SECURITY_ICON
#include FILEOPEN_ICON
#include FILEEDIT_ICON
#include NONE_ICON

static char *fallback[] = {
	"*background: LightGray",
	"*Frame.shadowType: Raised",
	"*font: -*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-*",
	"*variablewidth*font: -adobe-helvetica-medium-r-normal--*-120-*",
	"*monospaced*font: -*-courier-medium-r-*-*-14-*-*-*-*-*-*",
	"*sans-serif*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*",
	"*serif*font: -*-times-medium-r-*-*-12-*-*-*-*-*-*",
	"<Message>WM_PROTOCOLS: WMProtocols()\n",
	NULL
};

static char *fallback_webview[] = {
	"*variablewidth*font: -adobe-helvetica-medium-r-normal--*-120-*",
	"*monospaced*font: -*-courier-medium-r-*-*-14-*-*-*-*-*-*",
	"*sans-serif*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*",
	"*serif*font: -*-times-medium-r-*-*-12-*-*-*-*-*-*",
	"<Message>WM_PROTOCOLS: WMProtocols()\n",

/*General resources */
"webview*font: -*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-*",

/* Class-specific resources */
"webview*MwCheck.background: white",
	"webview*MwMenuBar.box_width: 1",
	"webview*MwMenuButton.highlight_on_enter: True",
	"webview*MwTooltip.Label.background: LightYellow",
	"webview*MwTooltip.borderWidth:	1",
	"webview*MwRichtext.background: white",
	"webview*MwFrame.shadowWidth: 1",
	"webview*MwFrame.shadowType: Raised",
	"webview*MwFrame.allowResize: True",

/* Resources for individual widgets */
"webview*tooltip.translations:	#override	\\n\
	<Enter>: webview-highlight(1)	\\n\
	<Leave>: webview-unhighlight(0)",

"webview*html.MwTextField.translations:	#override	\\n\
	<Key>Return: form_done()		\\n\
	<Key>Escape: form_reset()		\\n\
	:<Key>Tab: form_next()		\\n\
	Ctrl<Key>n: form_next()		\\n\
	Ctrl<Key>p: form_previous()		\\n\
	<Btn1Down>: form_select()",

"webview*html.Text.translations: #override	\\n\
	:<Key>Tab: form_next()		\\n\
	<Btn1Down>: form_select()",

"webview*html.translations:	#override	\\n\
	<Key>space: 	page_down()	\\n\
	<Key>BackSpace: page_up()	\\n\
	<Key>Page_Down: page_down()	\\n\
	<Key>Page_Up: 	page_up()	\\n\
	<Key>Down: 		scroll_down()	\\n\
	<Key>Up: 		scroll_up()	\\n\
	:<Key>Left:		scroll_left()	\\n\
	:<Key>Right:	scroll_right()",
	NULL
};

#define left_click Button1
#define right_click Button3

FORCEINLINE size_t str_length(ui_form_t field) {
	size_t len = 0;
	String value = TextFieldGetString(field);
	if (value) {
		len = strlen(value);
		free(value);
	}
	return len;
}

FORCEINLINE ui_bool str_is_regex(const char *pattern, ui_str_t match) {
	return regex_match(pattern, (const char *)match) == 0;
}

FORCEINLINE ui_bool is_ValidEmail(ui_str_t text) {
	const char *emailRegEx = "A[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*@(?:[a-z0-9]"
		"(?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?";
	return str_is_regex(emailRegEx, text);
}

FORCEINLINE ui_bool is_ValidUrl(ui_str_t text) {
	return str_is_regex("(http|https)://((\\w)*|([0-9]*)|([-|_])*)+([\\.|/]((\\w)*|([0-9]*)|([-|_])*))+", text);
}

ui_bool is_ValidPassword(ui_form_t field) {
	ui_field_str(passwordValue, field);
	ui_bool capitalResult = str_is_regex(".*[A-Z]+.*", passwordValue);
	ui_bool smallResult = str_is_regex(".*[a-z]+.*", passwordValue);
	ui_bool numberResult = str_is_regex(".*[0-9]+.*", passwordValue);

	return capitalResult && smallResult && numberResult;
}

static FORCEINLINE ui_bool is_MinLength(ui_form_t text, ui_field form) {
	return (0 == form.min) ? (ui_bool)true : (int)str_length(text) >= form.min;
}

static FORCEINLINE ui_bool is_MaxLength(ui_form_t text, ui_field form) {
	return (0 == form.max) ? (ui_bool)true : (int)str_length(text) <= form.max;
}

static FORCEINLINE ui_bool is_EmailValid(ui_form_t field, ui_field form) {
	ui_field_str(value, field);
	return form.kind == field_email ? is_ValidEmail(value) : (ui_bool)true;
}

static FORCEINLINE ui_bool is_UrlValid(ui_form_t field, ui_field form) {
	ui_field_str(value, field);
	return form.kind == field_url ? is_ValidUrl(value) : (ui_bool)true;
}

static FORCEINLINE ui_bool is_PasswordValid(ui_form_t text, ui_field form) {
	return (form.kind == field_secret) ? is_ValidPassword(text) : (ui_bool)true;
}

FORCEINLINE ui_bool str_field_valid(ui_form_t field, ui_field form) {
	ui_bool minimunLengthValidly = is_MinLength(field, form);
	ui_bool maximumLengthValidity = is_MaxLength(field, form);
	ui_bool emailValidity = is_EmailValid(field, form);
	ui_bool passwordValidity = is_PasswordValid(field, form);
	ui_bool urlValidity = is_UrlValid(field, form);
	return (minimunLengthValidly && maximumLengthValidity && emailValidity && passwordValidity && urlValidity);
}

static void hotkey_action(Widget w, XEvent *event, String *params, Cardinal *num_params) {
	trace;
	if (*num_params == 0)
		return;

	ui_t *ui = (ui_t *)main_athena_info->app;
	menu_bar_t *menubar = main_athena_info->bar_info;
	int i, r, number_items, number_menu = menubar->num_menus;
	for (r = 0; r < number_menu; r++) {
		menu_t *menu = &(menubar->menus[r]);
		number_items = menu->num_items;
		menuitem_t *items = menu->items;
		for (i = 0; i < number_items; i++) {
			if (strcmp(params[0], items[i].alphaKey) == 0) {
				ui->self = items[i].index;
				if (items[i].action)
					items[i].action(ui, items[i].data);
				return;
			}
		}
	}

	fprintf(stderr, "Unknown hotkey action: %s\n", params[0]);
}

static void _hotkey(ats_t *ats, int k) {
	if (k > 123)
		return;

	ui_t *ui = (ui_t *)ats->app;
	menu_bar_t *menubar = ats->bar_info;
	int i, r, number_items, number_menu = menubar->num_menus;
	for (r = 0; r < number_menu; r++) {
		menu_t *menu = &(menubar->menus[r]);
		menuitem_t *items = menu->items;
		number_items = menu->num_items;
		for (i = 0; i < number_items; i++) {
			if (items[i].alphaKey) {
				if (k == items[i].alphaKey[0]
					|| k == toupper(items[i].alphaKey[0])) {
					ui->self = items[i].index;
					ui->ats = ats;
					if (items[i].action)
						items[i].action(ui, items[i].data);
					return;
				}
			}
		}
	}
}

void ats_icon_set(ats_t *ui, const char *filepath) {
	XpmAttributes xpm_attr;
	XWMHints wm_hints;
	int status;

	if (ui->use_icon) {
		/* Create the pixmaps accordingly
	 	 * All the pixmaps MUST have the same height and width */
		xpm_attr.depth = XDefaultDepth(ui->dpy, ui->screen);
		xpm_attr.valuemask = XpmSize | XpmDepth;
		status = XpmCreatePixmapFromData(ui->dpy, ui->win,
			ui->use_icon,
			&ui->icon_pixmap,
			&ui->icon_mask,
			&xpm_attr
		);
	} else {
		// Load XPM icon
		xpm_attr.valuemask = 0; // No special attributes
		status = XpmReadFileToPixmap(ui->dpy, ui->win,
			filepath,
			&ui->icon_pixmap,
			&ui->icon_mask,
			&xpm_attr
		);
	}

	if (status != XpmSuccess) {
		fprintf(stderr, "Failed to load XPM file: %s\n", filepath);
		return;
	}

	ui->icon_set = 1;

	// Set the icon pixmap and mask
	wm_hints.flags = IconPixmapHint | IconMaskHint;
	wm_hints.icon_pixmap = ui->icon_pixmap;
	wm_hints.icon_mask = ui->icon_mask;
	XSetWMHints(ui->dpy, ui->win, &wm_hints);
}

static void WMProtocols(Widget w, XEvent *ev, String *params, Cardinal *nparams) {
	if (ev->type == ClientMessage
		&& !strcmp(XGetAtomName(XtDisplay(w), ev->xclient.message_type), "WM_PROTOCOLS")) {
	}
}

void verify_form(Widget self, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	ui_field form, *fill = (ui_field *)ui->app->app_array;
	int len, i, numFields = ui->app->code;
	ui_form_cb verify = (ui_form_cb)ui->user_data;
	char *value;
	Widget field;

	for (i = 0; i < numFields; i++) {
		form = fill[i];
		field = (Widget)form.index;
		char error[100] = {0};
		if (str_field_valid(field, form)) {
		} else {
			switch (form.kind) {
				case field_number:
					snprintf(error, sizeof(error), "Error: number must be between %d or %d digits",
						form.min, form.max);
					break;
				case field_text:
					snprintf(error, sizeof(error), "Error: length overflow %d or underflow %d",
						form.max, form.min);
					break;
				case field_secret:
					snprintf(error, sizeof(error), "Error: secret aleast 1 cap, 1 num and minimum %d",
						form.min);
					break;
				case field_email:
					snprintf(error, sizeof(error), "Error: invalid Email");
					break;
			}

			XtVaSetValues(ui->statusLine, XtNlabel, error, XtNforeground, 0xFF0000, NULL);
			XtVaSetValues(field, XtNborderColor, 0xFF0000, NULL);
			return;
		}

		XtVaSetValues(ui->statusLine, XtNlabel, "", NULL);
		XtVaSetValues(field, XtNborderColor, 0x008000, NULL);
		if (verify) {
			char status[260] = {0};
			if (verify(ui->app, form.ID, value, status)) {
				XtVaSetValues(ui->statusLine, XtNlabel, status, NULL);
			} else {
				// Error
				XtVaSetValues(ui->statusLine, XtNlabel, status, NULL);
				return;
			}
		}
	}

	ats_cancel(self);
}

static void alert_prompt_cb(Widget self, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	Button *buttons = (Button *)ui->app->app_array;
	int i, numFields = ui->app->code;

	for (i = 0; i < numFields; i++) {
		if (self == buttons[i].result) {
			ui->app->code = buttons[i].ID;
			ui->app->is_alert = false;
			ats_cancel(self);
			break;
		}
	}
}

FORCEINLINE void cancel_form(Widget self, XtPointer client, XtPointer data) {
	ats_cancel(self);
}

static void tab_editable_field(Widget self, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	ui_field *fill = (ui_field *)ui->app->app_array;
	ui_form_t field;
	int i, numFields = ui->app->code;
	for (i = 0; i < numFields; i++) {
		if (self == fill[i].index)
			break;
	}

	XtVaSetValues(self, XtNdisplayCaret, False, NULL);
	field = ((i + 1) == numFields) ? fill[0].index : fill[i + 1].index;
	XtSetKeyboardFocus((Widget)ui->app->app_data, field);
	XtVaSetValues(field, XtNdisplayCaret, True, NULL);
}

void verify_field(Widget self, XtPointer client, XtPointer data) {
	Arg wargs[10];
	ats_t *ui = (ats_t *)client;
	TextFieldReturnStruct *ret = (TextFieldReturnStruct *)data;
	printf("changed: string = %s\n", ret->string);
	//	XtSetArg(wargs[0], XtNlabel, "Error: invalid input!");
	//	XtSetValues(ui->statusLine, wargs, 1);
}

ats_wnd ats_field_set(ats_wnd on, ats_wnd alignto, char *initial, float x, float y, float width,
	ui_field_type kind, ats_t *ui) {
	ats_wnd text = XtVaCreateManagedWidget("sans-serif", textfieldWidgetClass, on,
		XtNwidth, width,
		XtNstring, initial,
		XtNborder, 0,
		XtNinsertPosition, 0,
		XtNdisplayCaret, False,
		XtNallowSelection, True,
		XtNresizable, True,
		XtNeditable, True,
		XtNecho, (kind == field_secret ? False : True),
		XtNborderColor, 0xDAA520,
		XtNx, x,
		XtNy, y,
		XtNheight, 20,
		XtNleft, XawChainLeft,
		XtNright, XawRubber,
		XtNtop, XawChainTop,
		XtNbottom, XawRubber,
		XtNfromVert, alignto,
		NULL);

	TextFieldAutoFocus(text);
	return text;
}

FORCEINLINE void ats_alignfield(ats_wnd self, ats_wnd to, bool is_vert) {
	if (is_vert)
		XtVaSetValues(self, XtNleft, XawRubber, XtNfromVert, to, NULL);
	else
		XtVaSetValues(self,	XtNright, XawRubber, XtNfromHoriz, to, NULL);
}

ats_wnd ats_image_set(ats_wnd on, char *pixmap) {
	Pixel color;

	XtVaGetValues(on, XtNbackground, &color, NULL);
	Pixmap pm = MwLoadPixmap(XtDisplay(on), color, pixmap);
	return XtVaCreateManagedWidget("image", mwImageWidgetClass, on,
		XtNbitmap, pm, XtNborderWidth, 0, NULL);
}

ats_wnd ats_button_set(ats_t *ats, ats_wnd on, ats_wnd alignto, const char *label,
	_platform_cb action, int is_vert) {
	Widget button;
	if (is_vert == 1) {
		button = XtVaCreateManagedWidget("command",
			commandWidgetClass, on,
			XtNlabel, label,
			XtNwidth, 80,
			XtNheight, 18,
			XtNborder, 1,
			XtNfont, main_athena_info->font_button,
			XtNborderColor, 0xDAA520,
			XtNborderWidth, 1,
			XtNleft, XawChainRight,
			XtNright, XawChainRight,
			XtNtop, XawChainRight,
			XtNbottom, XawChainRight,
			XtNfromVert, alignto,
			NULL);
	} else {
		button = XtVaCreateManagedWidget("command",
			commandWidgetClass, on,
			XtNlabel, label,
			XtNwidth, 80,
			XtNheight, 18,
			XtNborder, 1,
			XtNfont, main_athena_info->font_button,
			XtNborderColor, 0xDAA520,
			XtNborderWidth, 1,
			XtNleft, XawChainRight,
			XtNright, XawChainRight,
			XtNtop, XawChainRight,
			XtNbottom, XawChainRight,
			XtNfromHoriz, alignto,
			NULL);
	}

	XtAddCallback(button, XtNcallback, action, ats);
	return button;
}

ats_wnd ats_buttons_set(ats_t *ats, ats_wnd on, ats_wnd alignto, const char *label,
	_platform_cb action, int is_vert, int number) {
	Widget button = XtVaCreateManagedWidget("command",
		commandWidgetClass, on,
		XtNlabel, label,
		XtNwidth, 80,
		XtNheight, 20,
		XtNborder, 0,
		XtNmargin, 1,
		XtNfont, main_athena_info->font_button,
		XtNborderColor, (ats->app->code > 2 ? 0x800000 : 0x8FAEC4),
		XtNborderWidth, 1,
		XtNx, ((number - 1) * 80),
		XtNy, ats->height - 26,
		XtNgridy, 2,
		XtNgridx, number,
		XtNleft, XtChainLeft,
		XtNright, XtChainLeft,
		XtNtop, XtChainLeft,
		XtNbottom, XtChainBottom,
		XtNfromHoriz, alignto,
		NULL);

	if (is_vert == 1 && !ats->buttons_vert) {
		ats->buttons_vert = true;
		XtVaSetValues(button,
			XtNleft, XtChainRight,
			XtNright, XtChainRight,
			XtNtop, XtChainRight,
			XtNbottom, XtChainBottom,
			XtNfromVert, alignto,
			NULL);
	}

	XtAddCallback(button, XtNcallback, action, ats);
	return button;
}

int ats_form(ats_t *ui, const char *title, Form *fill, int numFields, ui_form_cb verify) {
	int i, y = 0, max_width = 0, spacing = 30;

	/* calculate form width based off longest field width */
	for (i = 0; i < numFields; i++) {
		if (fill[i].width > max_width)
			max_width = fill[i].width;
	}

	ui->user_data = verify;
	ui->app->code = numFields;
	ui->use_icon = none_xpm;
	/* calculate form height based off number of text fields provided */
	if (ats_window(ui, title, max_width + 20, numFields * 48, -1)) {
		Widget text = NULL, form = ats_formwindow_set(ui->topLevel);
		for (i = 0; i < numFields; i++) {
			/* Setup spacing between each `textfield` with `caption/label` */
			y += spacing;
			if (fill[i].caption)
				text = ats_labelfull_set("label", form, text, fill[i].caption, 6, y - 12, True);

			text = XtVaCreateManagedWidget("sans-serif", textfieldWidgetClass, form,
				XtNwidth, fill[i].width,
				XtNstring, fill[i].value,
				XtNborder, 0,
				XtNinsertPosition, 0,
				XtNdisplayCaret, False,
				XtNallowSelection, True,
				XtNeditable, True,
				XtNecho, (fill[i].kind != field_secret ? True : False),
				XtNborderColor, 0xDAA520,
				XtNbackground, 0xFFFFFF,
				XtNx, 5,
				XtNy, y - 5,
				XtNheight, 18,
				XtNleft, XtChainLeft,
				XtNfromVert, text,
				NULL);

			XtAddCallback(text, XtNactivateCallback, verify_field, ui);
			XtAddCallback(text, XtNvalueChangedCallback, verify_field, ui);
			TextFieldAutoFocus(text);
			XtAddCallback(text, XtNtabCallback, tab_editable_field, ui);
			//XtAddCallback(text, XtNmodifyVerifyCallback, editable_field, ui);
			fill[i].index = text;
		}

		ats_wnd button = ats_button_set(ui, form, text, "Confirm", verify_form, 1);
		text = ats_button_set(ui, form, button, "Cancel", cancel_form, 1);

		/* Setup statusline area in form for `error` feedback*/
		ui->statusLine = ats_statusline_set(form, text, "Fill out", 0, 0, ui->width - 20);
		ats_foreground_set(ui->statusLine, "dark green");

		/* Store provided `Form` for `verify_form` button click verification process */
		ui->app->app_array = (void **)fill;
		ui->app->app_data = (void *)form;
		ui->app->name = ui->title;
		ui->app->wnd = ui->topLevel;
		ui->app->ats = ui;
		ui->wnd = ui->topLevel;
		return 1;
	}

	return 0;
}

static size_t line_count(const char *text, const char *seps, int *count) {
	char *last, *tok, *data;
	int i;
	size_t max_line_size = 0, temp = 0;
	*count = 0;
	data = strdup(text);
	for (tok = strtok(data, seps); tok != NULL; tok = strtok(NULL, seps)) {
		temp = strlen(tok);
		if (temp > max_line_size)
			max_line_size = temp;
		(*count)++;
	}

	free(data);
	return max_line_size;
}

void ats_center(ats_wnd window) {
	Position x, y;
	Dimension w1, h1;
	Window root;
	int rx, ry, rw, rh, rb, rd;

	XtVaGetValues(window,
		XtNwidth, &w1,
		XtNheight, &h1, NULL);
	XGetGeometry(XtDisplay(window), DefaultRootWindow(XtDisplay(window)),
		&root, &rx, &ry, &rw, &rh, &rb, &rd);
	x = (rw - w1) / 2;
	y = (rh - h1) / 2;
	XtVaSetValues(window,
		XtNx, x,
		XtNy, y, NULL);
}

static void close_clicked(Widget self, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	ui->app->is_alert = false;
	ats_cancel(self);
}

void ats_about_box(char *pixmap, char *message) {
	Widget topbox, label, command, image;
	int i = -1, textLines = 0;
	ats_t ui = {0};
	size_t width = line_count(message, "\n", &textLines);

	ui.use_icon = info_xpm;
	if (ats_window(&ui, "About", (width * 8), (textLines * 45), -1)) {
		topbox = XtVaCreateManagedWidget("topbox",
			mwRudegridWidgetClass, ui.topLevel,
			XtNxLayout, "10 60 4 50% 60 50% 4 60 10",
			XtNyLayout, "4 50% 60 50% 4 22 10",
			NULL);
		image = ats_image_set(topbox, pixmap);
		XtVaSetValues(image, XtNgridx, 1, XtNgridy, 2, NULL);
		label = XtVaCreateManagedWidget("label",
			labelWidgetClass, topbox,
			XtNgridx, 3,
			XtNgridy, 1,
			XtNgridWidth, 7,
			XtNgridHeight, 3,
			NULL);
		MwLabelSet(label, message);
		XtVaSetValues(label, XtNfont, main_athena_info->font, NULL);
		command = XtVaCreateManagedWidget("command",
			commandWidgetClass, topbox,
			XtNgridx, 4,
			XtNgridy, 5,
			XtNlabel, "OK",
			XtNfont, main_athena_info->font_button,
			XtNborderWidth, 6,
			XtNbackground, 0x724F37,
			XtNwidth, 80,
			NULL);

		XtAddCallback(command, XtNcallback, close_clicked, &ui);

		ui.app->app_data = topbox;
		ui.app->name = ui.title;
		ui.app->wnd = ui.topLevel;
		ui.app->ats = &ui;
		ui.app->is_alert = true;
		ui.wnd = ui.topLevel;

		ats_center(ui.wnd);
		ats_active(&ui);
		ats_destroy(&ui);
	}
}

int ats_message_box(const char *title, const char *message,	Button *buttons, int numButtons) {
	ats_t ui = {0};
	int i = -1, but_h, but_w, textLines = 0;
	char layout[256] = "100%";
	size_t width = line_count(message, "\n", &textLines);

	but_w = numButtons == 1 ? 14 : (numButtons * 14);
	but_h = numButtons == 1 ? 18 : (numButtons * 6);
	if (numButtons > 2) {
		ui.use_icon = security;
	} else if (numButtons == 2) {
		ui.use_icon = why_xpm;
	} else {
		ui.use_icon = info_xpm;
	}

	if (ats_window(&ui, title, (width * 9) + but_w,
		(textLines * 42) + but_h, -1)) {
		ui.app->code = numButtons;

		for (i = 0; i< numButtons; i++)
			strncat(layout, " 70", sizeof(layout) - strlen(layout) - 1);

		strncat(layout, " 0", sizeof(layout) - strlen(layout) - 1);
		ats_wnd grid = XtVaCreateManagedWidget("topbox",
			gridboxWidgetClass, ui.wnd,
			XtNyLayout, "100% 10 20",
			XtNxLayout, layout,
			NULL);
		Widget text = NULL, prompt = ats_boxwindow_set(grid);
		text = XtVaCreateManagedWidget("label", labelWidgetClass, prompt,
			XtNlabel, _((String)message),
			XtNgridWidth, ui.app->code + 1,
			XtNborder, 0,
			XtNfont, main_athena_info->font,
			XtNborderWidth, 0,
			XtNmargin, 0,
			XtNgridx, 0,
			XtNgridy, 0,
			XtNleft, XawChainLeft,
			XtNfromVert, text, NULL);

		for (i = 0; i < numButtons; i++) {
			buttons[i].ID = i + 1;
			text = ats_buttons_set(&ui, grid, text, buttons[i].label, alert_prompt_cb, true, i + 1);
			buttons[i].result = text;
		}

		ui.app->app_data = prompt;
		ui.app->app_array = (void **)buttons;
		ui.app->name = ui.title;
		ui.app->wnd = ui.topLevel;
		ui.app->ats = &ui;
		ui.app->is_alert = true;
		ui.wnd = ui.topLevel;

		//ats_center(ui.wnd);
		ats_active(&ui);
		i = ui.app->code;
		ats_destroy(&ui);
	}

	return i;
}

FORCEINLINE void ats_background_set(ats_wnd on, char *to_color) {
	XColor xcolor;
	MwAllocNamedColor(XtDisplay(on), to_color, &xcolor);
	XtVaSetValues(on, XtNbackground, xcolor.pixel, NULL);
}

FORCEINLINE void ats_foreground_set(ats_wnd on, char *to_color) {
	XColor xcolor;
	MwAllocNamedColor(XtDisplay(on), to_color, &xcolor);
	XtVaSetValues(on, XtNforeground, xcolor.pixel, NULL);
}

FORCEINLINE ats_wnd ats_label_set(ats_wnd on, char *label) {
	return XtVaCreateManagedWidget("label", labelWidgetClass, on, XtNlabel, label, NULL);
}

FORCEINLINE ats_wnd ats_labelfull_set(char *tag, ats_wnd on, ats_wnd alignto,
	char *label, int x, int y, bool is_vert) {
	return XtVaCreateManagedWidget(tag, labelWidgetClass, on, XtNlabel, label,
		XtNfont, main_athena_info->font,
		XtNborder, 0,
		XtNborderWidth, 0,
		//XtNwidth, width,
		XtNheight, 9,
		XtNx, x,
		XtNy, y,
		XtNleft, XtChainLeft,
		(is_vert ? XtNfromVert : XtNfromHoriz), alignto, NULL);
}

FORCEINLINE ats_wnd ats_tabs_set(ats_wnd on) {
	return XtVaCreateManagedWidget("tabs", mwTabsWidgetClass, on, XtNgridy, 1, NULL);
}

FORCEINLINE ats_wnd ats_boxwindow_set(ats_wnd on) {
	return XtVaCreateManagedWidget("box", boxWidgetClass, on, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_boxspace_set(ats_wnd on, int vertical, int horizontal) {
	return XtVaCreateManagedWidget("box", boxWidgetClass, on,
		XtNborderWidth, 0,
		XtNvSpace, vertical,
		XtNhSpace, horizontal, NULL);
}

FORCEINLINE ats_wnd ats_formwindow_set(ats_wnd on) {
	return XtVaCreateManagedWidget("form", formWidgetClass, on,	XtNborderWidth, 0, NULL);
}

FORCEINLINE void ats_callback_set(ats_wnd on, _platform_cb action, void *with) {
	XtAddCallback(on, XtNcallback, action, with);
}

FORCEINLINE ats_wnd ats_statusline_set(ats_wnd on, ats_wnd alignto, const char *initial, int x, int y, int width) {
	return XtVaCreateManagedWidget("status", labelWidgetClass, on,
		XtNwidth, width,
		XtNlabel, initial,
		XtNfont, main_athena_info->font,
		XtNborder, 0,
		XtNborderWidth, 0,
		XtNheight, 10,
		XtNx, x,
		XtNy, y,
		XtNgridy, y,
		XtNfromVert, alignto, NULL);
}

static Boolean source_changed = FALSE;
FORCEINLINE void SourceChanged(Widget w, XtPointer  junk, XtPointer garbage) {
	XtRemoveCallback(w, XtNcallback, SourceChanged, NULL);
	source_changed = TRUE;
}

FORCEINLINE void ResetSourceChanged(Widget widget) {
	XtAddCallback(XawTextGetSource(widget), XtNcallback, SourceChanged, NULL);
	source_changed = FALSE;
}

void ats_editfile(ats_wnd self, XtPointer parent, XtPointer filepath) {
	ats_t ui = {0};
	Cardinal num_args = 0;
	Arg arglist[10];

	if (filepath == NULL) {
		ats_cancel(self);
		return;
	}

	ui.use_icon = editor_xpm;
	if (ats_window(&ui, filepath, 400, 400, false)) {
		XtSetArg(arglist[num_args], XtNtype, XawAsciiFile); num_args++;
		XtSetArg(arglist[num_args], XtNeditType, XawtextEdit); num_args++;
		XtSetArg(arglist[num_args], XtNscrollVertical, XawtextScrollWhenNeeded); num_args++;
		XtSetArg(arglist[num_args], XtNscrollHorizontal, XawtextScrollWhenNeeded); num_args++;
		XtSetArg(arglist[num_args], XtNwidth, 400); num_args++;
		XtSetArg(arglist[num_args], XtNheight, 400); num_args++;
		XtSetArg(arglist[num_args], XtNshowGrip, True); num_args++;
		XtSetArg(arglist[num_args], XtNstring, filepath); num_args++;
		XtSetArg(arglist[num_args], XtNfont, main_athena_info->font); num_args++;
		XtSetArg(arglist[num_args],	XtNborderWidth, 0); num_args++;
		XtSetArg(arglist[num_args],	XtNborder, 0); num_args++;
		XtSetArg(arglist[num_args],	XtNborderColor, 0xDAA520); num_args++;
		XtSetArg(arglist[num_args], XtNbackground, 0xFFFFFF); num_args++;
		Widget textwindow = XtCreateManagedWidget("editWindow", asciiTextWidgetClass,
			ui.topLevel, arglist, num_args);

		XtSetValues(textwindow, arglist, num_args);
		ResetSourceChanged(textwindow);

		ui.wnd = ui.topLevel;
		ui.app->wnd = ui.topLevel;
		ui.app->app_data = textwindow;
		ui.app->name = filepath;
		ui.app->ats = &ui;

		if (parent) {
			ats_t *ats = (ats_t *)parent;
			XtUnmapWidget(ats->topLevel);
			ats_cancel(self);
		}

		ats_active(&ui);
		ats_destroy(&ui);
	}
}

void ats_save_dialog(ui_t *self, _platform_cb save_handler) {
	//XawAsciiSaveAsFile(XawTextGetSource(self), data);
	//ResetSourceChanged(self);
}

void ats_open_dialog(ui_t *self, _platform_cb open_handler) {
	ats_t ui = {0};
	char *filter = "*";
	char *dir = "./";
	char *initial = "";

	ui.use_icon = fld_open_xpm;
	if (ats_window(&ui, "Open Dialog", 400, 400, false)) {
		Widget fileSelect = XtVaCreateManagedWidget("fileSelector",
			fileSelectWidgetClass, ui.topLevel, NULL, 0);

		if (open_handler)
			XtAddCallback(fileSelect, XtNcallback, open_handler, &ui);
		else
			XtAddCallback(fileSelect, XtNcallback, (XtCallbackProc)ats_editfile, &ui);

		FileSelectSet(fileSelect, dir, filter, initial);
		ui.app->app_data = (void *)fileSelect;
		ui.app->name = "Select File";
		ui.app->wnd = ui.topLevel;
		ui.app->ats = &ui;
		ats_active(&ui);
		ats_destroy(&ui);
	}
}

void ats_cancel(ats_wnd self) {
	Display *display = XtDisplayOfObject(self);
	Window win = XtWindowOfObject(self);
	XEvent ev;

	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = win;
	ev.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", false);
	ev.xclient.data.l[1] = CurrentTime;
	XSendEvent(display, win, False, NoEventMask, &ev);
	XSync(display, False);
}

void ats_active(ats_t *ui) {
	XtAppContext context = XtWidgetToApplicationContext((Widget)ui->app->app_data);
	if (!ui->dpy) {
		ui->dpy = XtDisplayOfObject((Widget)ui->app->app_data);
		XtRealizeWidget(ui->app->wnd);
		XtActionsRec fileprompt_actions[] = {
			{"WMProtocols", WMProtocols},
		};

		XtAppAddActions(context,
			fileprompt_actions, XtNumber(fileprompt_actions));

		/* set up to handle quits */
		XInternAtom(ui->dpy, "WM_PROTOCOLS", False);
		ui->wmDeleteMessage = XInternAtom(ui->dpy, "WM_DELETE_WINDOW", False);
		XtOverrideTranslations(ui->app->wnd,
			XtParseTranslationTable("<Message>WM_PROTOCOLS: WMProtocols()"));

		ui->win = XtWindow(ui->app->wnd);
		(void)XSetWMProtocols(ui->dpy, ui->win, (Atom *)&ui->wmDeleteMessage, 1);
		if (!ui->icon_set)
			ats_icon_set(ui, ATHENA_ICON);

		XStoreName(ui->dpy, ui->win, ui->app->name);
		XMapWindow(ui->dpy, ui->win);
	} else {
		XStoreName(ui->dpy, ui->win, ui->app->name);
		XFlush(ui->dpy);
	}

	for (;;) {
		XtAppNextEvent(context, &ui->xev);
		XtDispatchEvent(&ui->xev);
		if (ui->xev.xclient.type == ClientMessage && !ui->app->is_alert
			&& ui->xev.xclient.data.l[0] == ui->wmDeleteMessage) {
			break;
		} else if (ui->webview_set && ui->xev.type == ConfigureNotify) {
			XConfigureEvent xce = ui->xev.xconfigure;
			/* This event type is generated for a variety of
			   happenings, so check whether the window has been
			   resized. */
			if (xce.width != ui->web->width) {
				int numtools = 4;
				ui->web->width = xce.width;
				if (ui->web->priv.inspector_window)
					XtResizeWidget((Widget)ui->web->priv.inspector_window, (ui->web->width - (38 * numtools)), 28, 1);

				//if (ui->web->priv.ats->user_data)
				//	XtMoveWidget((Widget)ui->web->priv.ats->user_data, ui->web->width - 38, 0);
			}
		}
	}
	XtUnrealizeWidget(ui->app->wnd);
}

static void ats_menu_cb(Widget self, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	menu_bar_t *menubar = ui->bar_info;
	int i, r, number_items, number_menu = menubar->num_menus;
	for (r = 0; r < number_menu; r++) {
		menu_t *menu = &(menubar->menus[r]);
		menuitem_t *items = menu->items;
		number_items = menu->num_items;
		for (i = 0; i < number_items; i++) {
			if (self == items[i].index) {
				ui->app->ats = ui;
				ui->app->wnd = ui->topLevel;
				ui->app->self = self;
				if (items[i].action)
					items[i].action(ui->app, items[i].data);
				return;
			}
		}
	}
}

int ats_menu_set(ats_t *ui, int num_menu, menuitem_t *items, int number_items, int menu_id, char *name) {
	int r = 0;
	menu_t *menu = &(ui->bar_info->menus[num_menu]);

	menu->num_items = number_items;
	menu->selected = none_selected;
	menu->menu_id = menu_id;
	menu->items = items;
	snprintf(menu->menu_name, sizeof(menu->menu_name), "%smenu", name);
	Widget menubutton = XtVaCreateManagedWidget("menubutton", mwMBButtonObjectClass,
		ui->bar_info->hMenubar,
		XtNlabel, _(name),
		XtNmenu_name, menu->menu_name,
		XtNborderWidth, 0, NULL);
	menu->hMenu = XtVaCreatePopupShell(menu->menu_name, mwMenuWidgetClass,
		ui->bar_info->hMenubar, NULL, 0);
	if (menubutton && menu->hMenu) {
		// add menu items
		for (r = 0; r < number_items; r++) {
			if (items[r].item_name == NULL && items[r].action == NULL) {
				XtVaCreateManagedWidget("-", mwLineMEObjectClass, menu->hMenu, NULL);
			} else {
				if (items[r].alphaKey != NULL) {
					char alphaKey[65] = {0};
					char alpha[65] = {0};
					snprintf(alphaKey, sizeof(alphaKey), "%s  Ctrl+%s", items[r].item_name, items[r].alphaKey);
					menubutton = XtVaCreateManagedWidget(items[r].item_name,
						mwLabelMEObjectClass, menu->hMenu,
						XtNlabel, _(alphaKey),
						XtNborderWidth, 0,
						XtNfont, (ui->font == NULL ? main_athena_info->font : ui->font),
						XtNbackground, 0xf0f0f0,
						XtNforeground, 0x909090,
						NULL);
					snprintf(alpha, sizeof(alpha),
						"<Ctrl>%s:	hotkey(%s)\n", items[r].alphaKey, items[r].item_name);
					strncat(ui->bar_info->trans, alpha,
						sizeof(ui->bar_info->trans) - strlen(ui->bar_info->trans) - 1);
				} else {
					menubutton = XtVaCreateManagedWidget(items[r].item_name,
						mwLabelMEObjectClass, menu->hMenu,
						XtNlabel, _(items[r].item_name),
						XtNborderWidth, 0,
						XtNfont, (ui->font == NULL ? main_athena_info->font : ui->font),
						XtNbackground, 0xf0f0f0,
						XtNforeground, 0x909090,
						NULL);
				}
				items[r].index = menubutton;
				XtAddCallback(menubutton, XtNcallback, ats_menu_cb, ui);
			}
		}

		// Apply translations to toplevel
		XtOverrideTranslations(ui->topLevel, XtParseTranslationTable(ui->bar_info->trans));
	}

	return r;
}

int ats_font_set(ats_t *ui, const char *font) {
	ui->font = XLoadQueryFont(XtDisplayOfObject(ui->topLevel), font);
	return ui->font != NULL;
}

int ats_menubar_set(ats_t *ui, int numof_menus) {
	if ((ui->bar_info = (menu_bar_t *)calloc(1, sizeof(menu_bar_t)))) {
		ui->bar_info->menubox = ats_grid_set(ats_windowgrid_set(ui, 26, 10));
		ui->bar_info->hMenubar = XtVaCreateManagedWidget("menubox",
			mwMenuBarWidgetClass, ui->bar_info->menubox,
			XtNborder, 0,
			XtNborderWidth, 0,
			XtNheight, 26,
			XtNbackground, 0xffffff,
			XtNgridx, 0,
			NULL);
		ui->bar_info->num_menus = numof_menus;
		ui->bar_info->menus = (menu_t *)calloc(1, GetNumMenus(ui->bar_info) * sizeof(menu_t));
		ui->bar_info->state = 0;
		return 1;
	}

	return 0;
}

void ats_title_set(ats_t *ui, const char *title) {
	Atom wm_Name = XInternAtom(ui->dpy, "_NET_WM_NAME", False);
	Atom utf8Str = XInternAtom(ui->dpy, "UTF8_STRING", False);

	Atom winType = XInternAtom(ui->dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom typeDialog = XInternAtom(ui->dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);

	XChangeProperty(ui->dpy, ui->win, wm_Name, utf8Str, 8, PropModeReplace, (const unsigned char *)title, (int)strlen(title));
	XChangeProperty(ui->dpy, ui->win, winType, XA_ATOM, 32, PropModeReplace, (unsigned char *)&typeDialog, 1);
}

static int _ATS_KEYCODES[124] = {XK_BackSpace,8,XK_Delete,127,XK_Down,18,XK_End,5,XK_Escape,27,XK_Home,2,XK_Insert,26,XK_Left,20,XK_Page_Down,4,XK_Page_Up,3,XK_Return,10,XK_Right,19,XK_Tab,9,XK_Up,17,XK_apostrophe,39,XK_backslash,92,XK_bracketleft,91,XK_bracketright,93,XK_comma,44,XK_equal,61,XK_grave,96,XK_minus,45,XK_period,46,XK_semicolon,59,XK_slash,47,XK_space,32,XK_a,65,XK_b,66,XK_c,67,XK_d,68,XK_e,69,XK_f,70,XK_g,71,XK_h,72,XK_i,73,XK_j,74,XK_k,75,XK_l,76,XK_m,77,XK_n,78,XK_o,79,XK_p,80,XK_q,81,XK_r,82,XK_s,83,XK_t,84,XK_u,85,XK_v,86,XK_w,87,XK_x,88,XK_y,89,XK_z,90,XK_0,48,XK_1,49,XK_2,50,XK_3,51,XK_4,52,XK_5,53,XK_6,54,XK_7,55,XK_8,56,XK_9,57};

int ats_handler(ats_t *ui) {
	XFlush(ui->dpy);
	while (true) {
		XtAppNextEvent(ui->app_con, &ui->xev);
		switch (ui->xev.type) {
			case KeyPress:
			case KeyRelease:
				int m = ui->xev.xkey.state;
				int k = XkbKeycodeToKeysym(ui->dpy, ui->xev.xkey.keycode, 0, 0);
				ui->mod = (!!(m & ControlMask)) | (!!(m & ShiftMask) << 1)
					| (!!(m & Mod1Mask) << 2) | (!!(m & Mod4Mask) << 3);
				if ((ui->xev.type == KeyPress) && k && (ui->mod & 1))
					_hotkey(ui, k);
				break;
			case ButtonPress:
			case ButtonRelease:
				ui->mouse = (ui->xev.type == ButtonPress);
				break;
			case MotionNotify:
				ui->x = ui->xev.xmotion.x, ui->y = ui->xev.xmotion.y;
				break;
			case Expose:
				XGetWindowAttributes(ui->dpy, ui->win, &(ui->bar_info->gwa));
				break;
			case ClientMessage:
				if (ui->xev.xclient.data.l[0] == ui->wmDeleteMessage)
					return 0;
			default:
				break;
		}
		XtDispatchEvent(&ui->xev);
	}

	return 0;
}

int ats_window(ats_t *ui, const char *title, int width, int height, int alloc_buffer) {
	int argc = 0;
	char **argv = NULL;

	ui->title = title;
	ui->width = (int)width;
	ui->height = (int)height;
	if (alloc_buffer == true) {
		ui->buf = malloc(ui->width * ui->height * sizeof(uint32_t));
		if (!ui->buf)
			return 0;
	}

	if (main_athena_info == NULL) {
		ui->topLevel = XtVaOpenApplication(&ui->app_con, ui->title, NULL, 0,
			&argc, argv, fallback, sessionShellWidgetClass,
			XtNwidth, ui->width, XtNheight, ui->height,
			XtNbeNiceToColormap, True, NULL, 0);

		// Register hotkey action
		XtActionsRec actions[] = {{"hotkey", hotkey_action}};
		XtAppAddActions(ui->app_con, actions, XtNumber(actions));
		XawFocusInstallActions(ui->app_con);
		ui->use_icon = athena;
		main_athena_info = ui;
		main_athena_info->wnd = ui->topLevel;
		main_athena_info->wnd = ui->topLevel;
	}

	ui->dpy = XtDisplayOfObject(main_athena_info->wnd);
	if (ui->dpy == NULL) {
		XtAppError(ui->app_con, "\n\tCannot connect to X server\n\n");
		return 0;
	}

	if (ui->buf) {
		ui->screen = DefaultScreen(ui->dpy);
		ui->root = RootWindow(ui->dpy, ui->screen);
		ui->win = XCreateSimpleWindow(ui->dpy, ui->root, 0, 0, ui->width, ui->height,
			0, BlackPixel(ui->dpy, ui->screen), WhitePixel(ui->dpy, ui->screen));
		ui->gc = XCreateGC(ui->dpy, ui->win, 0, 0);
	} else {
		if (ui != main_athena_info) {
			ui->topLevel = XtVaAppInitialize(&ui->app_con, (ui->webview_set ? "webview" : ui->title), NULL, 0,
				&argc, argv, (ui->webview_set ? fallback_webview : fallback),
				XtNbeNiceToColormap, False,
				XtNwidth, ui->width,
				XtNheight, ui->height, NULL, 0);
		}

		XtRealizeWidget(ui->topLevel);
		ui->win = XtWindow(ui->topLevel);
	}

	XSelectInput(ui->dpy, ui->win, ExposureMask | KeyPressMask | KeyReleaseMask
		| ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);

	ui->wmDeleteMessage = XInternAtom(ui->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(ui->dpy, ui->win, &ui->wmDeleteMessage, 1);
	if (alloc_buffer == -1) {
		XSizeHints hints;
		hints.flags = PSize | PMinSize | PMaxSize;
		hints.min_width = hints.max_width = hints.base_width = width;
		hints.min_height = hints.max_height = hints.base_height = height;
		XSetWMNormalHints(ui->dpy, ui->win, &hints);
	}

	ats_icon_set(ui, ATHENA_ICON);
	XStoreName(ui->dpy, ui->win, ui->title);
	XMapWindow(ui->dpy, ui->win);

	if (!ui->buf) {
		ui->wnd = ui->topLevel;
		XSync(ui->dpy, 0);
	} else {
		ui->img = XCreateImage(ui->dpy, DefaultVisual(ui->dpy, 0), 24, ZPixmap, 0,
			(char *)ui->buf, ui->width, ui->height, 32, 0);
		XSync(ui->dpy, ui->win);
	}

	if (!main_athena_info->font) {
		MwInitFormat(ui->dpy);
		ui->font = XLoadQueryFont(ui->dpy, "lucidasans-8");
		ui->font_button = XLoadQueryFont(ui->dpy, lucida);
		main_athena_info->font = ui->font;
		main_athena_info->font = ui->font_button;
	}

	return 1;
}

FORCEINLINE ats_wnd ats_frame_set(ats_wnd on, XtShadowType shadowType, int shadowWidth) {
	return XtVaCreateManagedWidget("frame", mwFrameWidgetClass, on,
		XtNshadowWidth, shadowWidth, XtNshadowType, shadowType, NULL);
}

FORCEINLINE ats_wnd ats_combo_set(ats_wnd on, char **data, int datasize) {
	return XtVaCreateManagedWidget("combo", mwComboWidgetClass, on,
		XtNborderWidth, 0, XtNcomboData, data, XtNcomboNData, datasize, NULL);
}

FORCEINLINE void ats_combofield_set(ats_wnd on, char *text) {
	MwComboTextChange(on, text);
}

FORCEINLINE ats_wnd ats_checkradio_set(ats_wnd on, char *label, bool is_radio) {
	ats_wnd cr;
	if (is_radio) {
		cr = XtVaCreateManagedWidget("checkradio",
			mwCheckWidgetClass, on,
			XtNlabel, label,
			XtNborderWidth, 0,
			XtNcheckStyle, MwRadioWin,
			XtNradioGroup, on,
			XtNradioStart, on,
			NULL);
	} else {
		cr = XtVaCreateManagedWidget("checkradio",
			mwCheckWidgetClass, on,
			XtNlabel, label,
			XtNcheckStyle, MwCheckWin,
			XtNborderWidth, 0,
			NULL);
	}

	return cr;
}

void ats_dragdrop_set(ats_t *ui, const char *mime, int slot, _platform_cb dropfunc, _platform_cb dragfunc) {
	if (slot > MAX_DRAGDROPS || !mime)
		return;

	ui->dragdrop[slot] = XInternAtom(XtDisplay(ui->topLevel), mime, False);
	XtVaSetValues(ui->topLevel, XtNdropTypes, ui->dragdrop, NULL);
	if (!ui->dragdrop_set) {
		ui->dragdrop_set = true;
		XtAddCallback(ui->topLevel, XtNdragCallback, dragfunc, NULL);
		XtAddCallback(ui->topLevel, XtNdropCallback, dropfunc, NULL);
	}
}

FORCEINLINE void ats_dragdrop_update(ats_t *ui, const char *mime, int slot) {
	if (slot > MAX_DRAGDROPS || !ui->dragdrop_set || !mime)
		return;

	ui->dragdrop[slot] = XInternAtom(XtDisplay(ui->topLevel), mime, False);
	XtVaSetValues(ui->topLevel, XtNdropTypes, ui->dragdrop, NULL);
}

FORCEINLINE ats_wnd ats_tooltip_set(ats_t *ui, ats_wnd on, char *tip) {
	if (!ui->tooltip)
		ui->tooltip = XtVaCreatePopupShell("tooltip", mwTooltipWidgetClass, ui->topLevel, NULL);

	MwTooltipAdd(ui->tooltip, on, _(tip));
	return ui->tooltip;
}

ats_wnd ats_toolbar_set(ats_t *ui, ats_wnd on, _platform_cb button, char *imagefile, char *tip, bool showtip) {
	Pixel color;

	XtVaGetValues(on, XtNbackground, &color, NULL);
	ats_wnd w = XtVaCreateManagedWidget("toolbar", commandWidgetClass, on,
		XtNforeground, color, NULL);

	XtVaSetValues(w, XtNbitmap, MwLoadPixmap(XtDisplay(on), color, imagefile), NULL);
	XtAddCallback(w, XtNcallback, button, (XtPointer)ui);
	if (tip && showtip)
		ats_tooltip_set(ui, w, tip);

	return w;
}

FORCEINLINE ats_wnd ats_gridtwo_set(ats_wnd on, int leftwidth) {
	snprintf(main_athena_info->layout, sizeof(main_athena_info->layout),
		"%d %s", leftwidth, "100%");
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNxLayout, main_athena_info->layout, XtNborder, 0, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_gridlayout_set(ats_wnd on, int x, int y, const char *xLayout, const char *yLayout) {
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNgridx, x,
		XtNgridy, y,
		(xLayout == NULL ? XtNborder : XtNxLayout), (xLayout == NULL ? 0 : xLayout),
		(yLayout == NULL ? XtNborderWidth : XtNyLayout), (yLayout == NULL ? 0 : yLayout),
		XtNresizable, True,
		NULL);
}

FORCEINLINE ats_wnd ats_gridbar_set(ats_wnd on, int width, int height, const char *xLayout) {
	return  XtVaCreateManagedWidget("rudegrid",
		mwRudegridWidgetClass, on,
		XtNheight, height,
		XtNwidth, width,
		XtNresizable, True,
		XtNborder, 1,
		XtNborderWidth, 1,
		XtNxLayout, xLayout, NULL);
}

FORCEINLINE ats_wnd ats_grid_set(ats_wnd on) {
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNxLayout, "100%", XtNborder, 0, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_gridfull_set(ats_wnd on, int bottomheight) {
	snprintf(main_athena_info->layout, sizeof(main_athena_info->layout),
		"%s %d", "100%", bottomheight);
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNyLayout, main_athena_info->layout, XtNborder, 0, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_gridthree_set(ats_wnd on, int topheight, int bottomheight) {
	snprintf(main_athena_info->layout, sizeof(main_athena_info->layout),
		"%d %s %d", topheight, "100%", bottomheight);
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNyLayout, main_athena_info->layout, XtNborder, 0, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_gridfour_set(ats_wnd on, int leftwidth, int rightwidth) {
	snprintf(main_athena_info->layout, sizeof(main_athena_info->layout),
		"%d %s %s %d", leftwidth, "50%", "50%", rightwidth);
	return XtVaCreateManagedWidget("rudegrid", mwRudegridWidgetClass, on,
		XtNxLayout, main_athena_info->layout, XtNborder, 0, XtNborderWidth, 0, NULL);
}

FORCEINLINE ats_wnd ats_windowgrid_set(ats_t *ui, int topheight, int bottomheight) {
	return ats_gridthree_set(ui->topLevel, topheight, bottomheight);
}

FORCEINLINE ats_wnd ats_tabwindow_set(ats_wnd on, char *tabmessage, _platform_cb tabfunc) {
	ats_wnd w = XtVaCreateManagedWidget("tabbing",
		labelWidgetClass, on, XtNlabel, tabmessage, NULL);

	ats_wnd tabbing = XtVaCreateManagedWidget("tabbing",
		mwTabbingWidgetClass, on, XtNgridy, 1, NULL);

	XtAddCallback(tabbing, XtNselectCallback, tabfunc, (XtPointer)w);
	return tabbing;
}

FORCEINLINE void ats_tabinsert(ats_wnd tab, char *title, int slot) {
	MwTabbingInsert(tab, title, slot);
}

void ats_close(ats_t *ui) {
	if (ui) {
		if (ui->bar_info) {
			free(ui->bar_info->menus);
			free(ui->bar_info);
			ui->bar_info = NULL;
		}

		if (ui->icon_pixmap)
			XFreePixmap(ui->dpy, ui->icon_pixmap);

		if (ui->icon_mask)
			XFreePixmap(ui->dpy, ui->icon_mask);

		if (ui->gc)
			XFreeGC(ui->dpy, ui->gc);

		if (ui->buf) {
			ui->img->data = NULL;
			XDestroyImage(ui->img);
			free(ui->buf);
			ui->buf = NULL;
		}

		if (ui->wnd)
			XtDestroyWidget(ui->wnd);

		if (ui->app_con)
			XtDestroyApplicationContext(ui->app_con);

		XDestroyWindow(ui->dpy, ui->win);
		ui = NULL;
	}
}

FORCEINLINE void ats_destroy(ats_t *ui) {
	XtDestroyWidget(ui->app->wnd);
	if (ui->icon_pixmap)
		XFreePixmap(ui->dpy, ui->icon_pixmap);

	if (ui->icon_mask)
		XFreePixmap(ui->dpy, ui->icon_mask);

	XtDestroyApplicationContext(ui->app_con);
}

int ats_graphics_loop(ats_t *ui) {
	XPutImage(ui->dpy, ui->win, ui->gc, ui->img, 0, 0, 0, 0, ui->width, ui->height);
	XFlush(ui->dpy);
	while (XPending(ui->dpy)) {
		XNextEvent(ui->dpy, &ui->xev);
		switch (ui->xev.type) {
			case ClientMessage:
				if (ui->xev.xclient.data.l[0] == ui->wmDeleteMessage)
					return -(ClientMessage);
				break;
			case ButtonPress:
			case ButtonRelease:
				ui->mouse = (ui->xev.type == ButtonPress);
				break;
			case MotionNotify:
				ui->x = ui->xev.xmotion.x, ui->y = ui->xev.xmotion.y;
				break;
			case KeyPress:
			case KeyRelease: {
				unsigned int i;
				int m = ui->xev.xkey.state;
				int k = XkbKeycodeToKeysym(ui->dpy, ui->xev.xkey.keycode, 0, 0);
				for (i = 0; i < 124; i += 2) {
					if (_ATS_KEYCODES[i] == k) {
						ui->keys[_ATS_KEYCODES[i + 1]] = (ui->xev.type == KeyPress);
						break;
					}
				}
				ui->mod = (!!(m & ControlMask)) | (!!(m & ShiftMask) << 1)
					| (!!(m & Mod1Mask) << 2) | (!!(m & Mod4Mask) << 3);
			} break;
		}
	}
	return 0;
}

void ats_sleep(int64_t ms) {
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

int64_t ats_time(void) {
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return time.tv_sec * 1000 + (time.tv_nsec / 1000000);
}

#include "webview-athena.c"

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
	if (!main_athena_info->bar_info) {
		ui->web->width = main_athena_info->width;
		ui->web->height = main_athena_info->height;
	}
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