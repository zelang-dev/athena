#include <gui.h>
#include "libsmallregex.h"
static volatile gui_info *main_gui_info = NULL;
static volatile bool main_gui_shutdown = false;

#if defined(__linux__) && !defined(X11_WINDOW_ICON)
#define X11_WINDOW_ICON "../share/icon_32x32.xpm"
#endif

FORCEINLINE size_t str_length(ui_form_t field) {
#if __APPLE__
	return cocoa_strlen((NSString)cocoa_send((id)field, "stringValue"));
#elif defined(__linux__)
	size_t len = 0;
	String value = TextFieldGetString(field);
	if (value) {
		len = strlen(value);
		free(value);
	}
	return len;
#else
	return GetWindowTextLength(field);
#endif
}

FORCEINLINE ui_bool str_is_regex(const char *pattern, ui_str_t match) {
#if __APPLE__
	return regex_match(pattern, (const char *)cocoa_tochar(match)) == 0;
#else
	return regex_match(pattern, (const char *)match) == 0;
#endif
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

// window icon
#include X11_WINDOW_ICON
static char *Xt_menu_separator = " ------ ";
static char *fallback[] = {
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
	"webview*MwMenuButton.highlight_on_enter: False",
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

#define GetNumFonts(X) 		X->num_fonts
#define LineHeight(X,Y,Z) 	((double)(X->size[1]+X->size[2])*(Y-Z))/(double)X->gwa.height
#define MenuName(X,Y) 		X->menus[Y].menu_name
#define MenuStart(X,Y) 		x_left+(X->menus[Y].x_start/(double)X->gwa.width)*(x_right-x_left)
#define MenuWidth(X,Y) 		((double)X->menus[Y].width/(double)window_info->gwa.width)*(x_right-x_left)
#define GetState(X) 		(X->state-1)

#define x_left 2
#define x_right 8.2
#define y_bot -1
#define y_top 68
#define left_click Button1
#define right_click Button3
#define open_menu 1
#define close_menu 3
#define run_command 4
#define left_menu_padding 10
#define top_menu_padding 0.1

struct Dimensions {
	//window
	unsigned int winMinWidth;
	unsigned int winMinHeight;
	//vertical space between lines
	unsigned int lineSpacing;
	unsigned int barHeight;
	//padding
	unsigned int pad_up;
	unsigned int pad_down;
	unsigned int pad_left;
	unsigned int pad_right;
	//button
	unsigned int btSpacing;
	unsigned int btMinWidth;
	unsigned int btMinHeight;
	unsigned int btLateralPad;
};

typedef struct ButtonData {
	const Button *button;
	GC *gc;
	XRectangle rect;
} ButtonData;

//these values can be changed to whatever you prefer
struct Dimensions dim = {400, 150, 5, 40, 25, 10, 30, 30, 20, 75, 25, 8};

typedef struct XPMstruct {
	char *pixdata;
	int width, height;
} XPMstruct;

//not the colour-depth, but the representation in memory
#define XPM_DEPTH 32

//fallback when no background-colour is given for transparency-supported XPM
#define TRANSPCOLOR 0x808080

//only suitable for XPMs with around max. 50 colours (single-character colour-lookups)
//max. 64-colour XPMs are supported! (one base64 char/colour)
static XPMstruct XPMtoPixels(char *source[], unsigned long bgcol) {
	int i, j, k, sourceindex, pixdataindex, width,
		height, colours, charpix, transpchar = 0xff;
	unsigned int colr[256], colg[256], colb[256];
	unsigned char colchar[256], colcode, colindex,
		transpr = (bgcol & 0xFF0000) / 65536,
		transpg = (bgcol & 0xFF00) / 256,
		transpb = bgcol & 0xFF;
	XPMstruct xpms;

	sscanf(source[0], "%d %d %d %d", &width, &height, &colours, &charpix);
	 //32 is sure to have enough room
	unsigned char *pixdata = malloc(width * height * XPM_DEPTH / 8);
	for (i = 0; i < colours; i++) {
		if (sscanf(source[1 + i], "%c%*s #%2X%2X%2X", &colchar[i], &colr[i], &colg[i], &colb[i]) != 4)
			transpchar = colchar[i];
	}

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			sourceindex = (i * width + j);
			colcode = source[colours + 1 + i][j];
			for (k = 0; k < colours; k++) {
				if (colcode == colchar[k])
					break;
			}

			colindex = k;
			pixdataindex = (i * width + j) * XPM_DEPTH / 8;
			//Blue 0x502040
			pixdata[pixdataindex + 0] = (colcode != transpchar) ? colb[colindex] : transpb;
			//Green
			pixdata[pixdataindex + 1] = (colcode != transpchar) ? colg[colindex] : transpg;
			//Red
			pixdata[pixdataindex + 2] = (colcode != transpchar) ? colr[colindex] : transpr;
			//Aplha - 0:fully transparent...0xFF:fully opaque /Xlib has no alpha-channel support, Xrender has transparency-support
			pixdata[pixdataindex + 3] = (colcode == transpchar) ? 0x00 : 0xff;
		}
	}

	xpms.pixdata = pixdata;
	xpms.width = width;
	xpms.height = height;
	return xpms;
}

static void gui_seticon(gui_info *ui, char *xpm[]) {
	ui->icon_set = 1;
	XPMstruct xpms = XPMtoPixels(xpm, TRANSPCOLOR);
	XImage *iconimg = XCreateImage(ui->dpy, CopyFromParent, DefaultDepth(ui->dpy, ui->screen),
		ZPixmap, 0, xpms.pixdata, xpms.width, xpms.height, XPM_DEPTH, 0);

	XInitImage(iconimg);
	Pixmap icon = XCreatePixmap(ui->dpy, ui->win, xpms.width, xpms.height, DefaultDepth(ui->dpy, ui->screen));
	if (!ui->gc)
		ui->gc = XCreateGC(ui->dpy, ui->win, 0, 0);

	XPutImage(ui->dpy, icon, ui->gc, iconimg, 0, 0, 0, 0, xpms.width, xpms.height);
	XWMHints *hints = XAllocWMHints();
	hints->flags = IconPixmapHint;
	hints->icon_pixmap = icon;
	XSetWMHints(ui->dpy, ui->win, hints);
	XDestroyImage(iconimg);
	XUnmapWindow(ui->dpy, ui->win);
	XMapWindow(ui->dpy, ui->win);
	XFree(hints);
}

static int load_font(XFontStruct *font_info, char *font, Display *dpy, GLuint font_base, int *size) {
	font_info = XLoadQueryFont(dpy, font);
	size[0] = font_info->ascent;
	size[1] = font_info->descent;
	if (!font_info) {
		return 1;
	}

	glXUseXFont(font_info->fid, font_info->min_char_or_byte2, font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1, font_base + font_info->min_char_or_byte2);
	return 0;
}

static int print_string(GLuint font_base, char *s) {
	if (!glIsList(font_base)) {
		return 1;
	}
	glPushAttrib(GL_TEXTURE_BIT);
	glListBase(font_base);
	glCallLists(strlen(s), GL_UNSIGNED_BYTE, (GLubyte *)s);
	glPopAttrib();
	return 0;
}

static int drawMenubar(menu_bar_t *window_info) {
	int i = 0, error = 0;
	double x, y;
	y = y_top - ((double)window_info->size[1] / (double)window_info->gwa.height) * (y_top - y_bot);
	char name[max_font_name_length * 2];
	while (i < (window_info->num_menus)) {
		glColor3f(0, 0, 0);
		x = MenuStart(window_info, i);
		glRasterPos2f(x, y);
		snprintf(name, sizeof(name) - 1, " %s ", MenuName(window_info, i));
		error += print_string(window_info->font_lists, name);
		++i;
	}

	y = y_top - LineHeight(window_info, y_top, y_bot);
	glColor3f(1, 1, 1);
	glBegin(GL_POLYGON);
	glVertex3f(x_left, y_top, 0);  glVertex3f(x_right, y_top, 0);
	glVertex3f(x_right, y, 0);   glVertex3f(x_left, y, 0);
	glEnd();
	return error;
}

static int drawMenu(menu_bar_t *window_info) {
	int error = 0, index = 0;
	double y, line_height, x;
	line_height = LineHeight(window_info, y_top, y_bot);
	y = (double)(2 * window_info->size[1] + window_info->size[2]);
	y = y * (y_top - y_bot);
	y = y / (double)window_info->gwa.height;
	y = y_top - y;
	x = MenuStart(window_info, GetState(window_info));
	char name[max_font_name_length * 2];
	while (index < (window_info->menus[GetState(window_info)].num_items)) {
		if (index == window_info->menus[GetState(window_info)].active) {
			glColor3f(1, 1, 1);
		} else {
			glColor3f(0, 0, 0);
		}
		glRasterPos2f(x, y);
		snprintf(name, sizeof(name) - 1, " %s ", window_info->menus[GetState(window_info)].items[index].item_name);
		error += print_string(window_info->font_lists, name);
		y -= (line_height);
		++index;
	}

	glColor3f(0.7, 0.7, 0.7);
	y = y_top - (window_info->menus[GetState(window_info)].num_items + 1) * line_height;
	glBegin(GL_POLYGON);
	glVertex3f(x, y, 0);
	glVertex3f(x + MenuWidth(window_info, GetState(window_info)) + 0.1, y, 0);
	glVertex3f(x + MenuWidth(window_info, GetState(window_info)) + 0.1, y_top - line_height, 0);
	glVertex3f(x, y_top - line_height, 0);
	glEnd();
	return error;
}

static int draw(menu_bar_t *window_info) {
	int error = 0;
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(x_left, x_right, y_bot, y_top, 1., 20.);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.);

	error = drawMenubar(window_info);

	if (window_info->state != 0) {
		error += drawMenu(window_info);
	}

	return error;
}

static int whichMenu(menu_bar_t *window_info, double x) {
	int i = window_info->num_menus - 1;
	while (i > 0 && (MenuStart(window_info, i) > x)) {
		--i;
	}
	return i + 1;
}

static int whichActive(menu_bar_t *window_info, double x, double y) {
	int index = 0;
	double y_cat, line_height, x_min, x_max;
	line_height = LineHeight(window_info, y_top, y_bot);
	y_cat = y_top - line_height;
	x_min = MenuStart(window_info, GetState(window_info));
	x_max = x_min + MenuWidth(window_info, GetState(window_info));
	if (y > y_cat || x < x_min || x > x_max) {
		return -1;
	}
	y_cat -= line_height;
	while (x > x_min && x < x_max && y < y_cat && index < window_info->menus[GetState(window_info)].num_items) {
		y_cat -= (line_height);
		++index;
	}
	return (index != window_info->menus[GetState(window_info)].num_items) ? index : -2;
}

static void WMProtocols(Widget w, XEvent *ev, String *params, Cardinal *nparams) {
	if (ev->type == ClientMessage
		&& !strcmp(XGetAtomName(XtDisplay(w), ev->xclient.message_type), "WM_PROTOCOLS")) {
	}
}

void verify_form(__GUI_FIELD__) {
	gui_info *ui = (gui_info *)client;
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

	gui_cancel(self);
}

FORCEINLINE void cancel_form(__GUI_FIELD__) {
	gui_cancel(self);
}

static FORCEINLINE void gain_ebitable_field(__GUI_FIELD__) {
	XtVaSetValues(self, XtNdisplayCaret, True, NULL);
}

static FORCEINLINE void lose_ebitable_field(__GUI_FIELD__) {
	XtVaSetValues(self, XtNdisplayCaret, False, NULL);
}

static void tab_ebitable_field(__GUI_FIELD__) {
	gui_info *ui = (gui_info *)client;
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

void verify_field(__GUI_FIELD__) {
	Arg wargs[10];
	gui_info *ui = (gui_info *)client;
	TextFieldReturnStruct *ret = (TextFieldReturnStruct *)data;
	printf("changed: string = %s\n", ret->string);
	//	XtSetArg(wargs[0], XtNlabel, "Error: invalid input!");
	//	XtSetValues(ui->statusLine, wargs, 1);
}

Widget Xt_field(Widget window, Widget alignto, char *area, float x, float y, float width,
	ui_field_type kind, gui_info *ui) {
	Widget text = XtVaCreateManagedWidget("sans-serif", textfieldWidgetClass, window,
		XtNwidth, width,
		XtNstring, area,
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
		XtNheight, 30,
		XtNleft, XawChainLeft,
		XtNright, XawRubber,
		XtNtop, XawChainTop,
		XtNbottom, XawRubber,
		XtNfromVert, alignto,
		NULL);

	XtAddCallback(text, XtNfocusCallback, gain_ebitable_field, NULL);
	XtAddCallback(text, XtNgainPrimaryCallback, gain_ebitable_field, NULL);
	XtAddCallback(text, XtNlosingFocusCallback, lose_ebitable_field, NULL);
	XtAddCallback(text, XtNlosePrimaryCallback, lose_ebitable_field, NULL);
	return text;
}

Widget Xt_text_field(gui_info *gui, ui_field_type kind, char *label, char *field,
	float x, float y, float width, uintptr_t tag) {
	Widget text;
	return text;
}

Widget Xt_form_button(gui_info *gui, char *title, XtCallbackProc action, int x) {
	Widget button;
	if (x == 1)
		button = XtVaCreateManagedWidget("command",
			commandWidgetClass, (Widget)gui->app->app_data,
			XtNlabel, title,
			XtNwidth, 80,
			XtNheight, 18,
			XtNborder, 1,
			XtNfont, gui->font,
			XtNborderColor, 0xDAA520,
			XtNborderWidth, 2,
			XtNleft, XawChainRight,
			XtNright, XawChainRight,
			XtNtop, XawChainRight,
			XtNbottom, XawChainRight,
			XtNfromVert, (Widget)gui->app->app_array,
			NULL);
	else
		button = XtVaCreateManagedWidget("command",
			commandWidgetClass, (Widget)gui->app->app_data,
			XtNlabel, title,
			XtNwidth, 80,
			XtNheight, 18,
			XtNborder, 1,
			XtNfont, gui->font,
			XtNborderColor, 0xDAA520,
			XtNborderWidth, 2,
			XtNleft, XawChainRight,
			XtNright, XawChainRight,
			XtNtop, XawChainRight,
			XtNbottom, XawChainRight,
			XtNfromHoriz, (Widget)gui->app->app_array,
			NULL);

	XtAddCallback(button, XtNcallback, action, gui);
	gui->app->app_array = (void **)button;
	return button;
}

int gui_form(gui_info *ui, const char *title, Form *fill, int numFields, ui_form_cb verify) {
	int i, y = 0, max_width = 0, spacing = 30;
	int argc = 0;
	char **argv = NULL;

	/* calculate form width based off longest field width */
	for (i = 0; i < numFields; i++) {
		if (fill[i].width > max_width)
			max_width = fill[i].width;
	}

	ui->skip_resize = true;
	ui->title = title;
	ui->width = max_width + 20;
	/* calculate form height based off number of text fields provided */
	ui->height = numFields * 48;
	ui->user_data = verify;
	ui->app->code = numFields;
	ui->topLevel = XtAppInitialize(&ui->app_con, title, NULL, 0,
		&argc, argv, fallback, NULL, 0);
	XtResizeWidget(ui->topLevel, ui->width, ui->height, 0);

	Widget text = NULL, form = XtCreateManagedWidget("form", formWidgetClass, ui->topLevel, NULL, 0);
	ui->font = XLoadQueryFont(XtDisplayOfObject(form), "lucidasans-8");
	ui->font_button = XLoadQueryFont(XtDisplayOfObject(form), "lucidasans-bold-8");
	for (i = 0; i < numFields; i++) {
		/* Setup spacing between each `textfield` with `caption/label` */
		y += spacing;
		if (fill[i].caption) {
			text = XtVaCreateManagedWidget("label", labelWidgetClass, form,
				XtNlabel, fill[i].caption,
				XtNborder, 0,
				XtNfont, ui->font,
				XtNborderWidth, 0,
				XtNheight, 9,
				XtNx, 6,
				XtNy, y - 12,
				XtNleft, XtChainLeft,
				XtNfromVert, text, NULL);
		}

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
			XtNx, 5,
			XtNy, y - 5,
			XtNheight, 18,
			XtNleft, XtChainLeft,
			XtNfromVert, text,
			NULL);

		XtAddCallback(text, XtNactivateCallback, verify_field, ui);
		XtAddCallback(text, XtNvalueChangedCallback, verify_field, ui);
		XtAddCallback(text, XtNfocusCallback, gain_ebitable_field, ui);
		XtAddCallback(text, XtNgainPrimaryCallback, gain_ebitable_field, ui);
		XtAddCallback(text, XtNlosingFocusCallback, lose_ebitable_field, ui);
		XtAddCallback(text, XtNlosePrimaryCallback, lose_ebitable_field, ui);
		XtAddCallback(text, XtNtabCallback, tab_ebitable_field, ui);
		//XtAddCallback(text, XtNmodifyVerifyCallback, ebitable_field, ui);
		fill[i].index = text;
	}

	ui->app->app_data = form;
	ui->app->app_array = (void **)text;

	Xt_form_button(ui, "Confirm", verify_form, 1);
	text = Xt_form_button(ui, "Cancel", cancel_form, 1);

	/* Setup statusline area in form for `error` feedback*/
	ui->statusLine = XtVaCreateManagedWidget("label", labelWidgetClass, form,
		XtNwidth, ui->width - 20,
		XtNlabel, "Fill out",
		XtNforeground, 0x008000,
		XtNfont, ui->font,
		XtNborder, 0,
		XtNborderWidth, 0,
		XtNheight, 10,
		XtNfromVert, text, NULL);

	/* Store provided `Form` for `verify_form` button click verification process */
	ui->app->name = ui->title;
	ui->app->wnd = ui->topLevel;
	ui->app->gui = ui;
	ui->wnd = ui->topLevel;
	ui->app->app_array = (void **)fill;
	return 1;
}

static Boolean source_changed = FALSE;
void SourceChanged(Widget w, XtPointer  junk, XtPointer garbage) {
	XtRemoveCallback(w, XtNcallback, SourceChanged, NULL);
	source_changed = TRUE;
}

void ResetSourceChanged(Widget widget) {
	XtAddCallback(XawTextGetSource(widget), XtNcallback, SourceChanged, NULL);
	source_changed = FALSE;
}

void gui_file(__GUI_FILE__) {
	gui_info ui = {0};
	Cardinal num_args = 0;
	Arg arglist[10];
	int argc = 0;
	char **argv = NULL;

	if (data == NULL) {
		gui_cancel(self);
		return;
	}

	ui.topLevel = XtAppInitialize(&ui.app_con, data, NULL, 0,
		&argc, argv, fallback, NULL, 0);

	XtSetArg(arglist[num_args], XtNtype, XawAsciiFile); num_args++;
	XtSetArg(arglist[num_args], XtNeditType, XawtextEdit); num_args++;
	XtSetArg(arglist[num_args], XtNwidth, 500); num_args++;
	XtSetArg(arglist[num_args], XtNheight, 500); num_args++;
	XtSetArg(arglist[num_args], XtNshowGrip, True); num_args++;
	XtSetArg(arglist[num_args], XtNstring, data); num_args++;
	Widget textwindow = XtCreateManagedWidget("editWindow", asciiTextWidgetClass,
		ui.topLevel, arglist, num_args);

	XtSetValues(textwindow, arglist, num_args);
	ResetSourceChanged(textwindow);

	ui.wnd = ui.topLevel;
	ui.app->wnd = ui.topLevel;
	ui.app->app_data = textwindow;
	ui.app->name = data;
	ui.app->gui = &ui;

	gui_info *gui = (gui_info *)client;
	XtUnmapWidget(gui->topLevel);
	gui_cancel(self);
	gui_active(ui);
	gui_destroy(ui);
}

void gui_save_dialog(__GUI_MENU__) {
	//XawAsciiSaveAsFile(XawTextGetSource(self), data);
	//ResetSourceChanged(self);
}

void gui_open_dialog(__GUI_MENU__) {
	gui_info ui = {0};
	char *filter = "*";
	char *dir = "./";
	char *initial = "";

	int argc = 0;
	char **argv = NULL;

	ui.topLevel = XtAppInitialize(&ui.app_con, "FilePrompt", NULL, 0,
		&argc, argv, fallback, NULL, 0);
	ui.width = 400;
	ui.height = 400;
	XtResizeWidget(ui.topLevel, ui.width, ui.height, 0);

	Widget fileSelect = XtVaCreateManagedWidget("fileSelector",
		fileSelectWidgetClass, ui.topLevel, NULL, 0);
	if (data)
		XtAddCallback(fileSelect, XtNcallback, (XtCallbackProc)data, &ui);
	else
		XtAddCallback(fileSelect, XtNcallback, (XtCallbackProc)gui_file, &ui);

	FileSelectSet(fileSelect, dir, filter, initial);
	ui.app->app_data = (void *)fileSelect;
	ui.app->name = "Select File";
	ui.app->wnd = ui.topLevel;
	ui.app->gui = &ui;
	gui_active(ui);
	gui_destroy(ui);
}

void gui_cancel(ui_wnd_t self) {
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

void gui_active(gui_info ui) {
	Display *ldpy = XtDisplayOfObject((Widget)ui.app->app_data);
	XtAppContext context = XtWidgetToApplicationContext((Widget)ui.app->app_data);
	ui.dpy = ldpy;
	XtRealizeWidget(ui.app->wnd);

	XtActionsRec fileprompt_actions[] = {
		{"WMProtocols", WMProtocols},
	};

	XtAppAddActions(context,
		fileprompt_actions, XtNumber(fileprompt_actions));

	/* set up to handle quits */
	XInternAtom(ldpy, "WM_PROTOCOLS", False);
	Atom code = XInternAtom(ldpy, "WM_DELETE_WINDOW", False);
	XtOverrideTranslations(ui.app->wnd,
		XtParseTranslationTable("<Message>WM_PROTOCOLS: WMProtocols()"));

	XSizeHints hints;
	ui.win = XtWindow(ui.app->wnd);
	(void)XSetWMProtocols(ldpy, ui.win, (Atom *)&code, 1);

	if (ui.skip_resize) {
		hints.flags = PSize | PMinSize | PMaxSize;
		hints.min_width = hints.max_width = hints.base_width = ui.width;
		hints.min_height = hints.max_height = hints.base_height = ui.height;
		XSetWMNormalHints(ldpy, ui.win, &hints);
	}

	if (!ui.icon_set) {
		ui.icon_set = 1;
		MwSetIcon(ui.topLevel, icon_32x32);
	}

	XStoreName(ldpy, ui.win, ui.app->name);
	XMapWindow(ui.dpy, ui.win);
	XEvent ev;
	for (;;) {
		XtAppNextEvent(context, &ev);
		XtDispatchEvent(&ev);
		if (ev.xclient.type == ClientMessage && ev.xclient.data.l[0] == code)
			break;
	}
	XtUnrealizeWidget(ui.app->wnd);
}

int gui_menu(gui_info *ui, int num_menu, menuitem_t *items, int number_items, int menu_id, char *name) {
	int r;
	menu_t *menu = &(ui->bar_info->menus[num_menu]);
	menu->num_items = number_items;
	menu->selected = none_selected;
	menu->menu_id = menu_id;
	menu->items = items;
	//menu->items = (menuitem_t *)calloc(1, menu->num_items * sizeof(menuitem_t));
	//if (memcpy(menu->items, items, menu->num_items * sizeof(menuitem_t)) == NULL) {
	//	return 0;
	//}

	if (!(r = snprintf(menu->menu_name, sizeof(menu->menu_name) - 1, "%s", name)))
		XtAppError(ui->app_con, "\tMenu failed\n");

	return r;
}

static void gui_querymenu(gui_info *ui) {
	int error = 0, index = 0, item_index = 0;
	double x = 0, width = 0, temp;
	XFontStruct *menu_font = XLoadQueryFont(ui->dpy, ui->bar_info->font_names);

	while (index < (ui->bar_info->num_menus)) {
		ui->bar_info->menus[index].x_start = x;
		x += XTextWidth(menu_font, ui->bar_info->menus[index].menu_name, strlen(ui->bar_info->menus[index].menu_name));
		ui->bar_info->menus[index].x_end = x;
		x += left_menu_padding;
		while (item_index < ui->bar_info->menus[index].num_items) {
			if (!ui->bar_info->menus[index].items[item_index].item_name
				&& !ui->bar_info->menus[index].items[item_index].action) {
				ui->bar_info->menus[index].items[item_index].item_name = Xt_menu_separator;
				temp = XTextWidth(menu_font, Xt_menu_separator, strlen(Xt_menu_separator));
			} else {
				temp = XTextWidth(menu_font, ui->bar_info->menus[index].items[item_index].item_name,
					strlen(ui->bar_info->menus[index].items[item_index].item_name));
				temp += 4;
			}
			width = (temp > width) ? temp : width;
			++item_index;
		}
		item_index = 0;
		ui->bar_info->menus[index].width = width;
		++index;
	}
	ui->bar_info->bar_ready = true;
}

int gui_menufont(gui_info *ui, const char *font) {
	ui->bar_info->font_names = strdup(font);
	ui->bar_info->font_lists = glGenLists(256);
	if (!glIsList(ui->bar_info->font_lists)) {
		XtAppError(ui->app_con, "\tfont list failure\n");
		return 0;
	}

	if (load_font(ui->bar_info->font_info, ui->bar_info->font_names,
		ui->dpy, ui->bar_info->font_lists, &ui->bar_info->size[1]) != 0) {
		XtAppError(ui->app_con, "\tfont load failure\n");
		return 0;
	}

	return 1;
}

static void gui_free(gui_info *ui) {
	int i;
	if (ui) {
		if (ui->bar_info) {
			free(ui->bar_info->font_names);
			free(ui->bar_info->menus);
			free(ui->bar_info->font_info);
			free(ui->bar_info->size);
			free(ui->bar_info);
			ui->bar_info = NULL;
		}

		if (ui->glc) {
			glXMakeCurrent(ui->dpy, None, NULL);
			glXDestroyContext(ui->dpy, ui->glc);
		}

		XDestroyWindow(ui->dpy, ui->win);
		if (ui->buf) {
			free(ui->buf);
			ui->buf = NULL;
		}

		ui = NULL;
	}
}

int gui_menubar(gui_info *ui, int numof_menus) {
	int index = 0;
	if ((ui->bar_info = (menu_bar_t *)calloc(1, sizeof(menu_bar_t)))) {
		ui->bar_info->num_menus = numof_menus;
		ui->bar_info->menus = (menu_t *)calloc(1, GetNumMenus(ui->bar_info) * sizeof(menu_t));
		ui->bar_info->num_fonts = 1;
		ui->bar_info->font_info = (XFontStruct *)malloc(GetNumFonts(ui->bar_info) * sizeof(XFontStruct));
		ui->bar_info->size = (int *)malloc(GetNumFonts(ui->bar_info) * 2 * sizeof(int));
		ui->bar_info->state = 0;
		return 1;
	}

	return 0;
}

int gui_handler(gui_info *ui) {
	int buttoncase, click_x, click_y, error, check;
	double x_limit, y_limit, x, y;
	ui_t *app = ui->app;
	if (!ui->bar_info->bar_ready)
		gui_querymenu(ui);

	glEnable(GL_DEPTH_TEST);
	while (1) {
		XNextEvent(ui->dpy, &ui->xev);
		switch (ui->xev.type) {
			case MotionNotify:
				if (ui->bar_info->state != 0) {
					x = x_left + ((double)ui->xev.xmotion.x / (double)ui->bar_info->gwa.width) * (x_right - x_left);
					y = y_top - ((double)ui->xev.xmotion.y / (double)ui->bar_info->gwa.height) * (y_top - y_bot);
					check = ui->bar_info->menus[GetState(ui->bar_info)].active;
					ui->bar_info->menus[GetState(ui->bar_info)].active = whichActive(ui->bar_info, x, y);
					if (check != ui->bar_info->menus[GetState(ui->bar_info)].active) {
						error = draw(ui->bar_info);
						glXSwapBuffers(ui->dpy, ui->win);
					}
				}
				break;
			case ButtonPress:
				x_limit = x_left + (ui->bar_info->menus[ui->bar_info->num_menus - 1].x_end / (double)ui->bar_info->gwa.width) * (x_right - x_left);
				y_limit = y_top - LineHeight(ui->bar_info, y_top, y_bot);
				x = x_left + ((double)ui->xev.xbutton.x / (double)ui->bar_info->gwa.width) * (x_right - x_left);
				click_x = (x < 0) ? -1 : (int)x;
				y = y_bot + ((double)(ui->bar_info->gwa.height) - (double)ui->xev.xbutton.y) * ((double)y_top - (double)y_bot) / ((double)ui->bar_info->gwa.height);
				click_y = (y < 0) ? -1 : (int)y;
				buttoncase = open_menu * (ui->xev.xbutton.button == left_click) * (ui->bar_info->state == 0) * (x < x_limit) * (y > y_limit);
				buttoncase += close_menu * (ui->bar_info->state != 0) * (whichActive(ui->bar_info, x, y) < 0);
				buttoncase += run_command * (ui->bar_info->state != 0) * (whichActive(ui->bar_info, x, y) >= 0);
				switch (buttoncase) {
					case open_menu:
						ui->bar_info->state = whichMenu(ui->bar_info, x);
						break;
					case close_menu:
						ui->bar_info->state = 0;
						break;
					case run_command:
						memset(app, 0, sizeof(ui_t));
						app->wnd = ui->topLevel;
						app->gui = ui;
						menuitem_t menu_active = ui->bar_info->menus[GetState(ui->bar_info)]
							.items[ui->bar_info->menus[GetState(ui->bar_info)].active];
						if (menu_active.action)
							menu_active.action(app, menu_active.data);
						break;
					default:
						break;
				}
			case Expose:
				XGetWindowAttributes(ui->dpy, ui->win, &(ui->bar_info->gwa));
				glViewport(0, 0, ui->bar_info->gwa.width, ui->bar_info->gwa.height);
				error = draw(ui->bar_info);
				if (error != 0) {
					fprintf(stderr, "\tfont failure: %d\n", error);
					return 0;
				}
				glXSwapBuffers(ui->dpy, ui->win);
				break;
			case ClientMessage:
				if (ui->xev.xclient.data.l[0] == ui->wmDeleteMessage) {
					return 0;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

static void setWindowTitle(const char *title, const Window *win, Display *dpy) {
	Atom wm_Name = XInternAtom(dpy, "_NET_WM_NAME", False);
	Atom utf8Str = XInternAtom(dpy, "UTF8_STRING", False);

	Atom winType = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom typeDialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);

	XChangeProperty(dpy, *win, wm_Name, utf8Str, 8, PropModeReplace, (const unsigned char *)title, (int)strlen(title));
	XChangeProperty(dpy, *win, winType, XA_ATOM,
		32, PropModeReplace, (unsigned char *)&typeDialog,
		1);
}

static void split(const char *text, const char *seps, char ***str, int *count) {
	char *last, *tok, *data;
	int i;
	*count = 0;
	data = strdup(text);

	for (tok = strtok(data, seps); tok != NULL; tok = strtok(NULL, seps))
		(*count)++;

	free(data);
	fflush(stdout);
	data = strdup(text);
	*str = (char **)malloc((size_t)(*count) * sizeof(char *));

	for (i = 0, tok = strtok(data, seps); tok != NULL; tok = strtok(NULL, seps), i++)
		(*str)[i] = strdup(tok);
	free(data);
}

static void computeTextSize(XFontSet *fs, char **texts, int size, unsigned int spaceBetweenLines,
	unsigned int *w, unsigned  int *h) {
	int i;
	XRectangle rect = {0,0,0,0};
	*h = 0;
	*w = 0;
	for (i = 0; i < size; i++) {
		Xutf8TextExtents(*fs, texts[i], (int)strlen(texts[i]), &rect, NULL);
		*w = (rect.width > *w) ? (rect.width) : *w;
		*h += rect.height + spaceBetweenLines;
		fflush(stdin);
	}
}

static void createGC(GC *gc, const Colormap *cmap, Display *dpy, const  Window *win,
	unsigned char red, unsigned char green, unsigned char blue) {
	float coloratio = (float)65535 / 255;
	XColor color;
	*gc = XCreateGC(dpy, *win, 0, 0);
	memset(&color, 0, sizeof(color));
	color.red = (unsigned short)(coloratio * red);
	color.green = (unsigned short)(coloratio * green);
	color.blue = (unsigned short)(coloratio * blue);
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy, *cmap, &color);
	XSetForeground(dpy, *gc, color.pixel);
}

static bool isInside(int x, int y, XRectangle rect) {
	if (x < rect.x || x >(rect.x + rect.width) || y < rect.y || y >(rect.y + rect.height))
		return false;
	return true;
}

int gui_message_box(ui_t *app, const char *title, const char *text, const Button *buttons, int numButtons) {
	gui_info ui = {0};
	// convert the text in list (to draw in multiply lines)
	char **text_splitted = NULL;
	int textLines = 0;
	split(text, "\n", &text_splitted, &textLines);

	ui.dpy = XOpenDisplay(NULL);
	if (ui.dpy == NULL) {
		fprintf(stderr, "Error opening display display.");
		return -1;
	}

	ui.screen = DefaultScreen(ui.dpy);
	ui.win = XCreateSimpleWindow(ui.dpy, RootWindow(ui.dpy, ui.screen), 0, 10, 400, 120, 0,
		BlackPixel(ui.dpy, ui.screen), WhitePixel(ui.dpy, ui.screen));

	XSelectInput(ui.dpy, ui.win, ExposureMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
	XMapWindow(ui.dpy, ui.win);

	//allow windows to be closed by pressing cross button (but it wont close - see ClientMessage on switch)
	Atom WM_DELETE_WINDOW = XInternAtom(ui.dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(ui.dpy, ui.win, &WM_DELETE_WINDOW, 1);

	// create the gc for drawing text
	XGCValues gcValues;
	gcValues.font = XLoadFont(ui.dpy, "7x13");
	gcValues.foreground = BlackPixel(ui.dpy, ui.screen);
	ui.gc = XCreateGC(ui.dpy, ui.win, GCFont + GCForeground, &gcValues);
	XUnmapWindow(ui.dpy, ui.win);

	// create fontset
	char **missingCharset_list = NULL;
	int i, missingCharset_count = 0;
	XFontSet fs;
	fs = XCreateFontSet(ui.dpy,
		"-*-*-medium-r-*-*-*-140-75-75-*-*-*-*",
		&missingCharset_list, &missingCharset_count, NULL);

	if (missingCharset_count) {
		fprintf(stderr, "Missing charsets :\n");
		for (i = 0; i < missingCharset_count; i++) {
			fprintf(stderr, "%s\n", missingCharset_list[i]);
		}
		XFreeStringList(missingCharset_list);
		missingCharset_list = NULL;
	}

	Colormap cmap = DefaultColormap(ui.dpy, ui.screen);

	//resize the window according to the text size
	unsigned int winW, winH;
	unsigned int textW, textH;

	//calculate the ideal window's size
	computeTextSize(&fs, text_splitted, textLines, dim.lineSpacing, &textW, &textH);
	unsigned int newWidth = textW + dim.pad_left + dim.pad_right;
	unsigned int newHeight = textH + dim.pad_up + dim.pad_down + dim.barHeight;
	winW = (newWidth > dim.winMinWidth) ? newWidth : dim.winMinWidth;
	winH = (newHeight > dim.winMinHeight) ? newHeight : dim.winMinHeight;

	//set windows hints
	XSizeHints hints;
	hints.flags = PSize | PMinSize | PMaxSize;
	hints.min_width = hints.max_width = hints.base_width = winW;
	hints.min_height = hints.max_height = hints.base_height = winH;

	XSetWMNormalHints(ui.dpy, ui.win, &hints);
	gui_seticon(&ui, icon_32x32);

	GC barGC;
	GC buttonGC;
	GC buttonGC_underPointer;
	GC buttonGC_onClick;                               // GC colors
	createGC(&barGC, &cmap, ui.dpy, &ui.win, RGB_WHITE);
	createGC(&buttonGC, &cmap, ui.dpy, &ui.win, RGB_GOLDEN_ROD);
	createGC(&buttonGC_underPointer, &cmap, ui.dpy, &ui.win, RGB_SILVER);
	createGC(&buttonGC_onClick, &cmap, ui.dpy, &ui.win, RGB_DIM_GRAY);

	/* setup the buttons data */
	ButtonData *btsData = (ButtonData *)malloc((size_t)numButtons * sizeof(ButtonData));

	int pass = 0;
	for (i = 0; i < numButtons; i++) {
		btsData[i].button = &buttons[i];
		btsData[i].gc = &buttonGC;
		XRectangle btTextDim;
		Xutf8TextExtents(fs, btsData[i].button->label, (int)strlen(btsData[i].button->label),
			&btTextDim, NULL);
		btsData[i].rect.width = (btTextDim.width < dim.btMinWidth) ? dim.btMinWidth :
			(btTextDim.width + 2 * dim.btLateralPad);
		btsData[i].rect.height = dim.btMinHeight;
		btsData[i].rect.x = winW - dim.pad_left - btsData[i].rect.width - pass;
		btsData[i].rect.y = textH + dim.pad_up + dim.pad_down + ((dim.barHeight - dim.btMinHeight) / 2);
		pass += btsData[i].rect.width + dim.btSpacing;
	}

	setWindowTitle(title, &ui.win, ui.dpy);
	XFlush(ui.dpy);

	bool quit = false;
	int res = -1;

	while (!quit) {
		XEvent e;
		XNextEvent(ui.dpy, &e);
		switch (e.type) {
			case MotionNotify:
			case ButtonPress:
			case ButtonRelease:
				for (i = 0; i < numButtons; i++) {
					btsData[i].gc = &buttonGC;
					if (isInside(e.xmotion.x, e.xmotion.y, btsData[i].rect)) {
						btsData[i].gc = &buttonGC_underPointer;
						if (e.type == ButtonPress && e.xbutton.button == Button1) {
							btsData[i].gc = &buttonGC_onClick;
							res = i + 1;
							quit = true;
						}
					}
				}
			case Expose:
				// draw the text in multiply lines
				for (i = 0; i < textLines; i++) {
					Xutf8DrawString(ui.dpy, ui.win, fs, ui.gc, dim.pad_left, dim.pad_up + i * (dim.lineSpacing + 18),
						text_splitted[i], (int)strlen(text_splitted[i]));
				}

				XFillRectangle(ui.dpy, ui.win, barGC, 0, textH + dim.pad_up + dim.pad_down, winW, dim.barHeight);
				for (i = 0; i < numButtons; i++) {
					XFillRectangle(ui.dpy, ui.win, *btsData[i].gc, btsData[i].rect.x, btsData[i].rect.y,
						btsData[i].rect.width, btsData[i].rect.height);

					XRectangle btTextDim;
					Xutf8TextExtents(fs, btsData[i].button->label, (int)strlen(btsData[i].button->label),
						&btTextDim, NULL);
					Xutf8DrawString(ui.dpy, ui.win, fs, ui.gc,
						btsData[i].rect.x + (btsData[i].rect.width - btTextDim.width) / 2,
						btsData[i].rect.y + (btsData[i].rect.height + btTextDim.height) / 2,
						btsData[i].button->label, (int)strlen(btsData[i].button->label));
				}
				XFlush(ui.dpy);
				break;
			case ClientMessage:
				break;
			default:
				break;
		}
	}

	for (i = 0; i < textLines; i++) {
		free(text_splitted[i]);
	}
	free(text_splitted);
	free(btsData);
	if (missingCharset_list)
		XFreeStringList(missingCharset_list);
	XDestroyWindow(ui.dpy, ui.win);
	XFreeFontSet(ui.dpy, fs);
	XFreeGC(ui.dpy, ui.gc);
	XFreeGC(ui.dpy, barGC);
	XFreeGC(ui.dpy, buttonGC);
	XFreeGC(ui.dpy, buttonGC_underPointer);
	XFreeGC(ui.dpy, buttonGC_onClick);
	XFreeColormap(ui.dpy, cmap);
	XCloseDisplay(ui.dpy);

	return res;
}

// clang-format off
static int _GUI_KEYCODES[124] = {XK_BackSpace,8,XK_Delete,127,XK_Down,18,XK_End,5,XK_Escape,27,XK_Home,2,XK_Insert,26,XK_Left,20,XK_Page_Down,4,XK_Page_Up,3,XK_Return,10,XK_Right,19,XK_Tab,9,XK_Up,17,XK_apostrophe,39,XK_backslash,92,XK_bracketleft,91,XK_bracketright,93,XK_comma,44,XK_equal,61,XK_grave,96,XK_minus,45,XK_period,46,XK_semicolon,59,XK_slash,47,XK_space,32,XK_a,65,XK_b,66,XK_c,67,XK_d,68,XK_e,69,XK_f,70,XK_g,71,XK_h,72,XK_i,73,XK_j,74,XK_k,75,XK_l,76,XK_m,77,XK_n,78,XK_o,79,XK_p,80,XK_q,81,XK_r,82,XK_s,83,XK_t,84,XK_u,85,XK_v,86,XK_w,87,XK_x,88,XK_y,89,XK_z,90,XK_0,48,XK_1,49,XK_2,50,XK_3,51,XK_4,52,XK_5,53,XK_6,54,XK_7,55,XK_8,56,XK_9,57};

int gui_window(gui_info *ui, const char *title, int width, int height, int buffered) {
	GLint att[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE,     24,
		GLX_STENCIL_SIZE,   8,
		GLX_RED_SIZE,       8,
		GLX_GREEN_SIZE,     8,
		GLX_BLUE_SIZE,      8,
		GLX_SAMPLE_BUFFERS, 0,
		GLX_SAMPLES,        0,
		None
	};
	int argc = 0;
	char **argv = NULL;

	ui->title = title;
	ui->width = (int)width;
	ui->height = (int)height;
	if (buffered == true) {
		ui->buf = malloc(ui->width * ui->height * sizeof(uint32_t));
		if (!ui->buf)
			return 0;
	}

	if (main_gui_info == NULL) {
		ui->wnd = XtOpenApplication(&ui->app_con, ui->title, NULL, 0, &argc, argv,
			fallback, sessionShellWidgetClass, NULL, 0);
		main_gui_info = ui;
	}

	ui->dpy = XtDisplayOfObject(main_gui_info->wnd);
	if (ui->dpy == NULL) {
		XtAppError(ui->app_con, "\n\tcannot connect to X server\n\n");
		return 0;
	}

	ui->screen = DefaultScreen(ui->dpy);
	if (!ui->buf) {
		ui->vi = glXChooseVisual(ui->dpy, ui->screen, att);
		if (ui->vi == NULL) {
			XtAppError(ui->app_con, "\n\tno appropriate visual found\n\n");
			return 0;
		}
	}

	ui->root = RootWindow(ui->dpy, ui->screen);
	ui->win = XCreateSimpleWindow(ui->dpy, ui->root, 0, 0, ui->width, ui->height, 0,
		BlackPixel(ui->dpy, ui->screen), WhitePixel(ui->dpy, ui->screen));

	if (ui->buf)
		ui->gc = XCreateGC(ui->dpy, ui->win, 0, 0);

	XSelectInput(ui->dpy, ui->win, ExposureMask | KeyPressMask | KeyReleaseMask
		| ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
	ui->wmDeleteMessage = XInternAtom(ui->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(ui->dpy, ui->win, &ui->wmDeleteMessage, 1);

	if (buffered == -1) {
		XSizeHints hints;
		hints.flags = PSize | PMinSize | PMaxSize;
		hints.min_width = hints.max_width = hints.base_width = width;
		hints.min_height = hints.max_height = hints.base_height = height;
		XSetWMNormalHints(ui->dpy, ui->win, &hints);
	}

	gui_seticon(ui, icon_32x32);
	XStoreName(ui->dpy, ui->win, ui->title);

	if (!ui->buf) {
		XSync(ui->dpy, 0);
		ui->glc = glXCreateContext(ui->dpy, ui->vi, NULL, GL_TRUE);
		glXMakeCurrent(ui->dpy, ui->win, ui->glc);
		ui->topLevel = XtAppCreateShell(ui->title, NULL, applicationShellWidgetClass, ui->dpy, NULL, 0);
		XtResizeWidget(ui->topLevel, ui->width, ui->height, 1);
		ui->app->wnd = ui->topLevel;
	} else {
		XSync(ui->dpy, ui->win);
		ui->img = XCreateImage(ui->dpy, DefaultVisual(ui->dpy, 0), 24, ZPixmap, 0,
			(char *)ui->buf, ui->width, ui->height, 32, 0);
	}

	ui->font = XLoadQueryFont(ui->dpy, "lucidasans-8");
	return 1;
}

#include "webview-x11.c"

FORCEINLINE void gui_close(gui_info *ui) {
	gui_free(ui);
}

FORCEINLINE void gui_destroy(gui_info ui) {
	XtDestroyApplicationContext(ui.app_con);
}

int gui_loop(gui_info *ui) {
	//XEvent ev;
	unsigned int i;
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
					int m = ui->xev.xkey.state;
					int k = XkbKeycodeToKeysym(ui->dpy, ui->xev.xkey.keycode, 0, 0);
				for (i = 0; i < 124; i += 2) {
					if (_GUI_KEYCODES[i] == k) {
						ui->keys[_GUI_KEYCODES[i + 1]] = (ui->xev.type == KeyPress);
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


void gui_sleep(int64_t ms) {
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

int64_t gui_time(void) {
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return time.tv_sec * 1000 + (time.tv_nsec / 1000000);
}

FORCEINLINE int gui_webview(gui_info *ui, const char *title, const char *url,
	int width, int height, bool showtoolbar) {
#if !defined(__APPLE__) || !defined(_WIN32)
	if (tls_init() != 0) {
		webview_debug("tls_init failed");
		return 0;
	}
#endif

	ui->web->url = url;
	ui->web->title = title;
	ui->web->width = width;
	ui->web->height = height;
	ui->web->resizable = 1;
	ui->web->showtoolbar = showtoolbar;
	ui->web->debug = 1;
	if (!main_gui_info->bar_info) {
		ui->web->width = main_gui_info->width;
		ui->web->height = main_gui_info->height;
	}

#if defined(__APPLE__)
	if (!main_gui_info->bar_info) {
		cocoa_set_with(main_gui_info->wnd, "setTitle:", (id)cocoa_str(main_gui_info->title));
		main_gui_info->window[0] = main_gui_info->wnd;
	}

	ui->webView[0] = main_gui_info->webView[0];
	ui->window[0] = main_gui_info->window[0];
	ui->delegate = main_gui_info->delegate;
	main_gui_info->web->title = title;
#endif
	return webview_create(ui, ui->web);
}

FORCEINLINE void gui_webactive(gui_info ui) {
#if defined(__APPLE__) || !defined(_WIN32)
	webview_loop(ui.web, 1);
#else
	ui.is_webview = 1;
	ui.wnd = ui.web->priv.hwnd;
	gui_handler(&ui);
	ui.is_webview = 0;
#endif
}

FORCEINLINE void gui_webdestroy(gui_info ui) {
	webview_exit(ui.web);
}

WEBVIEW_API int webview_run(const char *title, const char *url, int width, int height) {
	gui_info ui = {0};
	gui_webview(&ui, title, url, width, height, true);
	gui_webactive(ui);
	gui_webdestroy(ui);
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