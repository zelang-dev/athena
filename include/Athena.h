#ifndef _ATHENA_H
#define _ATHENA_H

#ifndef C_API
#	define C_API extern
#endif

#ifndef trace
#	define Statement(s) do {	\
			s	\
		}	while (0)
#	define trace 		Statement(fprintf(stderr, "%s:%d Trace \n", __FILE__, __LINE__);)
#endif

#ifndef casting
	/* Cast ~val~, a `non-pointer` to `pointer` like value,
	makes reference if variable. */
#	define casting(val) (void *)((ptrdiff_t)(val))
#endif

#ifndef ats_sizeof
#	define ats_sizeof(data) (sizeof(data)/sizeof(data[0]))
#endif

//#ifdef __linux__
#define lucida "lucidasans-bold-8"
#define helvetica "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-*"
#define times "-*-times-medium-r-*-*-12-*-*-*-*-*-*"
#define courier "-*-courier-medium-r-*-*-14-*-*-*-*-*-*"
#define __ATS_MENU__ 	ui_t *self, void *data
#define __ATS_FILE__ 	Widget self, XtPointer client, XtPointer data
#define __ATS_FIELD__ 	__ATS_FILE__
#define __ATS_WEBVIEW__ __ATS_FILE__
#define _DEFAULT_SOURCE 1
#define ARROW_SCROLLBAR 1

#define RGB_COLOR(r, g, b)		(r),(g),(b)
#define RGB_BLACK				0,0,0
#define RGB_WHITE				255,255,255
#define RGB_RED					255,0,0
#define RGB_LIME				0,255,0
#define RGB_BLUE				0,0,255
#define RGB_YELLOW				255,255,0
#define RGB_CYAN				0,255,255
#define RGB_MAGENTA				255,0,255
#define RGB_SILVER				192,192,192
#define RGB_GRAY				128,128,128
#define RGB_MAROON				128,0,0
#define RGB_OLIVE				128,128,0
#define RGB_GREEN				0,128,0
#define RGB_PURPLE				128,0,128
#define RGB_TEAL				0,128,128
#define RGB_NAVY				0,0,128
#define RGB_MAROON				128,0,0
#define RGB_DARK_RED			139,0,0
#define RGB_BROWN				165,42,42
#define RGB_FIREBRICK			178,34,34
#define RGB_CRIMSON				220,20,60
#define RGB_RED					255,0,0
#define RGB_TOMATO				255,99,71
#define RGB_CORAL				255,127,80
#define RGB_INDIAN_RED			205,92,92
#define RGB_LIGHT_CORAL			240,128,128
#define RGB_DARK SALMON			233,150,122
#define RGB_SALMON				250,128,114
#define RGB_LIGHT_SALMON		255,160,122
#define RGB_ORANGE_RED			255,69,0
#define RGB_DARK_ORANGE			255,140,0
#define RGB_ORANGE				255,165,0
#define RGB_GOLD				255,215,0
#define RGB_DARK_GOLDEN_ROD		184,134,11
#define RGB_GOLDEN_ROD			218,165,32
#define RGB_PALE_GOLDEN_ROD		238,232,170
#define RGB_DARK_KHAKI			189,183,107
#define RGB_KHAK				240,230,140
#define RGB_OLIVE				128,128,0
#define RGB_YELLOW				255,255,0
#define RGB_YELLOW_GREEN		154,205,50
#define RGB_DARK_OLIVE_GREEN	85,107,47
#define RGB_OLIVE_DRAB			107,142,35
#define RGB_LAWN_GREEN			124,252,0
#define RGB_CHARTREUSE			127,255,0
#define RGB_GREEN_YELLOW		173,255,47
#define RGB_DARK_GREEN			0,100,0
#define RGB_GREEN				0,128,0
#define RGB_FOREST_GREEN		34,139,34
#define RGB_LIME				0,255,0
#define RGB_LIME_GREEN			50,205,50
#define RGB_LIGHT_GREEN			144,238,144
#define RGB_PALE_GREEN			152,251,152
#define RGB_DARK_SEA_GREEN		143,188,143
#define RGB_MEDIUM_SPRING_GREEN	0,250,154
#define RGB_SPRING_GREEN		0,255,127
#define RGB_SEA_GREEN			46,139,87
#define RGB_MEDIUM_AQUA_MARINE	102,205,170
#define RGB_MEDIUM_SEA_GREEN	60,179,113
#define RGB_LIGHT_SEA_GREEN		32,178,170
#define RGB_DARK_SLATE_GRAY		47,79,79
#define RGB_TEAL				0,128,128
#define RGB_DARK_CYAN			0,139,139
#define RGB_AQUA				0,255,255
#define RGB_CYAN				0,255,255
#define RGB_LIGHT_CYAN			224,255,255
#define RGB_DARK_TURQUOISE		0,206,209
#define RGB_TURQUOISE			64,224,208
#define RGB_MEDIUM_TURQUOISE	72,209,204
#define RGB_PALE_TURQUOISE		175,238,238
#define RGB_AQUA_MARINE			127,255,212
#define RGB_POWDER_BLUE			176,224,230
#define RGB_CADET_BLUE			95,158,160
#define RGB_STEEL_BLUE			70,130,180
#define RGB_CORN_FLOWER_BLUE	100,149,237
#define RGB_DEEP_SKY_BLUE		0,191,255
#define RGB_DODGER_BLUE			30,144,255
#define RGB_LIGHT_BLUE			173,216,230
#define RGB_SKY_BLUE			135,206,235
#define RGB_LIGHT_SKY_BLUE		135,206,250
#define RGB_MIDNIGHT_BLUE		25,25,112
#define RGB_NAVY				0,0,128
#define RGB_DARK_BLUE			0,0,139
#define RGB_MEDIUM_BLUE			0,0,205
#define RGB_BLUE				0,0,255
#define RGB_ROYAL_BLUE			65,105,225
#define RGB_BLUE_VIOLET			138,43,226
#define RGB_INDIGO				75,0,130
#define RGB_DARK_SLATE_BLUE		72,61,139
#define RGB_SLATE_BLUE			106,90,205
#define RGB_MEDIUM_SLATE_BLUE	123,104,238
#define RGB_MEDIUM_PURPLE		147,112,219
#define RGB_DARK_MAGENTA		139,0,139
#define RGB_DARK_VIOLET			148,0,211
#define RGB_DARK_ORCHID			153,50,204
#define RGB_MEDIUM_ORCHID		186,85,211
#define RGB_PURPLE				128,0,128
#define RGB_THISTLE				216,191,216
#define RGB_PLUM				221,160,221
#define RGB_VIOLET				238,130,238
#define RGB_FUCHSIA				255,0,255
#define RGB_ORCHID				218,112,214
#define RGB_MEDIUM_VIOLET_RED	199,21,133
#define RGB_PALE_VIOLET_RED		219,112,147
#define RGB_DEEP_PINK			255,20,147
#define RGB_HOT_PINK			255,105,180
#define RGB_LIGHT_PINK			255,182,193
#define RGB_PINK				255,192,203
#define RGB_ANTIQUE_WHITE		250,235,215
#define RGB_BEIGE				245,245,220
#define RGB_BISQUE				255,228,196
#define RGB_BLANCHED_ALMOND		255,235,205
#define RGB_WHEAT				245,222,179
#define RGB_CORN_SILK			255,248,220
#define RGB_LEMON_CHIFFON		255,250,205
#define RGB_LIGHT_GOLDEN_ROD_YELLOW	250,250,210
#define RGB_LIGHT_YELLOW		255,255,224
#define RGB_SADDLE_BROWN		139,69,19
#define RGB_SIENNA				160,82,45
#define RGB_CHOCOLATE			210,105,30
#define RGB_PERU				205,133,63
#define RGB_SANDY_BROWN			244,164,96
#define RGB_BURLY_WOOD			222,184,135
#define RGB_TAN					210,180,140
#define RGB_ROSY_BROWN			188,143,143
#define RGB_MOCCASIN			255,228,181
#define RGB_NAVAJO_WHITE		255,222,173
#define RGB_PEACH_PUFF			255,218,185
#define RGB_MISTY_ROSE			255,228,225
#define RGB_LAVENDER_BLUSH		255,240,245
#define RGB_LINEN				250,240,230
#define RGB_OLD_LACE			253,245,230
#define RGB_PAPAYA_WHIP			255,239,213
#define RGB_SEA_SHELL			255,245,238
#define RGB_MINT_CREAM			245,255,250
#define RGB_SLATE_GRAY			112,128,144
#define RGB_LIGHT_SLATE_GRAY	119,136,153
#define RGB_LIGHT_STEEL_BLUE	176,196,222
#define RGB_LAVENDER			230,230,250
#define RGB_FLORAL_WHITE		255,250,240
#define RGB_ALICE_BLUE			240,248,255
#define RGB_GHOST_WHITE			248,248,255
#define RGB_HONEYDEW			240,255,240
#define RGB_IVORY				255,255,240
#define RGB_AZURE				240,255,255
#define RGB_SNOW				255,250,250
#define RGB_BLACK				0,0,0
#define RGB_DIM_GRAY			105,105,105
#define RGB_GREY				128,128,128
#define RGB_DARK_GREY			169,169,169
#define RGB_SILVER				192,192,192
#define RGB_LIGHT_GREY			211,211,211
#define RGB_GAINSBORO			220,220,220
#define RGB_WHITE_SMOKE			245,245,245
#define RGB_WHITE				255,255,255

#define GetNumMenus(X) 		X->num_menus
#define item_name_limit 	50
#define none_selected 		-1
#define no_menu 			-1
#define max_font_name_length 60
#define MAX_MSGBUTTONS  11
#define MAX_DRAGDROPS  25

#ifndef FORCEINLINE
#	if defined(_MSC_VER) && !defined(__clang__)
#		define FORCEINLINE __forceinline
#	elif defined(__GNUC__)
#		if defined(__STRICT_ANSI__)
#			define FORCEINLINE __inline__ __attribute__((always_inline))
#		else
#			define FORCEINLINE inline __attribute__((always_inline))
#		endif
#	elif defined(__BORLANDC__) || defined(__DMC__) || defined(__SC__)	\
		|| defined(__WATCOMC__) || defined(__LCC__) || defined(__DECC)
#		define FORCEINLINE __inline
#	else /* No inline support. */
#		define FORCEINLINE
#	endif
#endif

#define __ATS_SEPERATOR__ 	0, 0, 0, 0, 0
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xresource.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Object.h>
#include <X11/Shell.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Xmu.h>
#include <X11/Xmu/Converters.h>
#include <X11/xpm.h>
#include <Mowitz.h>

#ifdef NATIVE_XAW
#	include <X11/Xaw/Box.h>
#	include <X11/Xaw/Paned.h>
#	include <X11/Xaw/Dialog.h>
#	include <X11/Xaw/Command.h>
#	include <X11/Xaw/Form.h>
#	include <X11/Xaw/AsciiText.h>

#	include <X11/Xaw/MenuButton.h>
#	include <X11/Xaw/Label.h>
#	include <X11/Xaw/Viewport.h>
#	include <X11/Xaw/List.h>
#	include <X11/Xaw/Scrollbar.h>
#	include <X11/Xaw/SimpleMenu.h>
#	include <X11/Xaw/SmeBSB.h>
#	include <X11/Xaw/SmeLine.h>
#	include <X11/Xaw/Repeater.h>
#	include <X11/Xaw/Traversal.h>
#else
#	include <Xaw95/Box.h>
#	include <Xaw95/Paned.h>
#	include <Xaw95/Dialog.h>
#	include <Xaw95/Command.h>
#	include <Xaw95/Form.h>
#	include <Xaw95/AsciiText.h>

#	include <Xaw95/MenuButton.h>
#	include <Xaw95/Label.h>
#	include <Xaw95/Viewport.h>
#	include <Xaw95/List.h>
#	include <Xaw95/Scrollbar.h>
#	include <Xaw95/SimpleMenu.h>
#	include <Xaw95/SmeBSB.h>
#	include <Xaw95/SmeLine.h>
#	include <Xaw95/Repeater.h>
#	include <Xaw95/Traversal.h>
#endif

#include <FileSelect.h>
#include <Gridbox.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>

typedef enum {
	field_text = 4900,
	field_number,
	field_date,
	field_email,
	field_secret,
	field_url,
	field_regex,
} ui_field_type;

enum {
	ID_ATS_ICON = 900,
	ID_ATS_MENU = 1000,
	ID_ATS_CONTROL = 2000,
	ID_ATS_CONFIRM,
	ID_ATS_CANCEL,
	ID_ATS_RESET,
	ID_ATS_STATUS = 3000,
	ID_ATS_ERROR,
	ID_ATS_VERIFIED,
	ID_ATS_STATIC = 4000,
};

/* Platform Window type */
typedef Widget ats_wnd;
/* Platform FONT type */
typedef XFontStruct *ats_font;
/* Platform Color type */
typedef Colormap ats_color;
/* Platform Menu type */
typedef Widget ats_menu;
/* Platform TextField type */
typedef Widget ui_form_t;
/* Platform string type */
typedef String ui_str_t;
/* Platform bool type */
typedef bool ui_bool;
#define ui_field_str(value, field)		String value = MwTextFieldGetString(field)

typedef struct Buttons_s {
	int ID;
	char *label;
	ats_wnd result;
} Button;

typedef struct Forms_s {
	uintptr_t ID;
	ui_field_type kind;
	char *caption;
	char *value;
	int width;
	int max;
	int min;
	ui_form_t index;
} Form;

typedef Form ui_field;
typedef struct ats_info_s ats_t;
typedef struct {
	int is_alert;
	/* App's custom code */
	unsigned long code;
	/* `Application` main Window handle */
	ats_wnd wnd;
	/* App's custom handle */
	void *app_data;
	void **app_array;
	/* App's custom title */
	const char *name;
	ats_t *ats;
	ats_wnd self;
} ui_t;

typedef struct hist hist_t;
struct webview_priv {
	Widget window;
	Widget scroller;
	Widget webview;
	Widget inspector_window;
	void **queue;
	int ready;
	int js_busy;
	int should_exit;
	ats_t *ats;
	hist_t *backhist, *forwhist;
};

typedef XtCallbackProc _platform_cb;
typedef void (*_menu_cb)(ui_t *self, void *data);
typedef void (*ui_file_cb)(ui_t *, const char *);
typedef bool (*ui_form_cb)(const ui_t *, uint32_t field_id, void *, char *err);
typedef struct {
	int menu_id;
	char *item_name;
	_menu_cb action;
	char *alphaKey;
	void *data;
	ats_menu index;
} menuitem_t;

typedef struct menu_s {
	menuitem_t *items;	// menu items
	int num_items;	// # of menu items
	int active;
	int selected;
	int menu_id;
	char menu_name[32];
	ats_menu hMenu;
	ats_font hFont;
	double x_start;
	double x_end;
	double width;
} menu_t;

typedef struct {
	int bar_ready;
	int state;
	int num_menus;
	/* main menu */
	menu_t *menus;
#if __linux__
	XWindowAttributes gwa;
	ats_menu menubox;
	char trans[512];
#endif
	char *font_names;
	ats_font font_info;
	ats_menu hMenubar;
} menu_bar_t;

typedef struct webview webview_t;
typedef void (*webview_external_invoke_cb_t)(webview_t *w, const char *arg);
struct webview {
	const char *url;
	const char *title;
	int width;
	int height;
	int resizable;
	int showtoolbar;
	int debug;
	webview_external_invoke_cb_t external_invoke_cb;
	struct webview_priv priv;
	ats_font statusline;
	void *userdata;
};

struct ats_info_s {
	menu_bar_t *bar_info;
	int width;
	int height;
	int x;
	int y;
	int mouse;
	int error;
	int mod;       /* mod is 4 bits mask, ctrl=1, shift=2, alt=4, meta=8 */
	int keys[256]; /* keys are mostly ASCII, but arrows are 17..20 */
	const char *title;
	uint32_t *buf;
	ats_wnd wnd;
	ats_font font;
	ats_color txtCr, bkCr;
	/* For passing data to custom `Window` handler routine */
	void *user_data;
	ui_t app[1];
	webview_t web[1];
#if __linux__
	bool webview_set;
	bool icon_set;
	bool dragdrop_set;
	bool buttons_vert;
	int screen;
	Colormap cmap;
	Window win, root;
	Pixmap icon_pixmap, icon_mask;
	Atom code, wmDeleteMessage, dragdrop[MAX_DRAGDROPS + 1];
	XEvent xev;
	Widget topLevel, statusLine, tooltip;
	Display *dpy;
	XVisualInfo *vi;
	XtAppContext app_con;
	GLXContext glc;
	GC gc;
	XImage *img;
	XFontStruct *font_button;
	char **use_icon, layout[32];
#endif
};

typedef Button ats_buttons[MAX_MSGBUTTONS];

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(X11_WINDOW_ICON)
#	define X11_WINDOW_ICON "../share/icon_32x32.xpm"
#endif

#include X11_WINDOW_ICON

C_API void ats_active(ats_t *ui);
C_API void ats_destroy(ats_t *ui);
C_API void ats_cancel(ats_wnd self);
C_API void ats_callback_set(ats_wnd on, _platform_cb action, void *with);

C_API ats_wnd ats_field_set(ats_wnd on, ats_wnd alignto, char *initial, float x, float y,
	float width, ui_field_type kind, ats_t *ui);
C_API int ats_form(ats_t *ui, const char *title, Form *fill, int numFields, ui_form_cb verify);

C_API int ats_message_box(const char *title, const char *message, Button *buttons, int numButtons);
C_API void ats_about_box(char *pixmap, char *message);
C_API void ats_center(ats_wnd window);

C_API ats_wnd ats_tooltip_set(ats_t *ui, ats_wnd on, char *tip);
C_API ats_wnd ats_toolbar_set(ats_t *ui, ats_wnd on, _platform_cb button, char *imagefile, char *tip);

C_API ats_wnd ats_grid_set(ats_wnd on);
C_API ats_wnd ats_gridtwo_set(ats_wnd on, int leftwidth);
C_API ats_wnd ats_gridthree_set(ats_wnd on, int topheight, int bottomheight);
C_API ats_wnd ats_gridfour_set(ats_wnd on, int leftwidth, int rightwidth);
C_API ats_wnd ats_gridfull_set(ats_wnd on, int bottomheight);

C_API ats_wnd ats_statusline_set(ats_wnd on, ats_wnd alignto, const char *initial, int y, int width);

C_API ats_wnd ats_frame_set(ats_wnd on, XtShadowType shadowType, int shadowWidth);
C_API ats_wnd ats_label_set(ats_wnd on, char *label);
C_API ats_wnd ats_labelfull_set(char *tag, ats_wnd on, ats_wnd alignto,
	char *label, int x, int y, bool is_vert);
C_API ats_wnd ats_checkradio_set(ats_wnd on, char *label, bool is_radio);
C_API ats_wnd ats_combo_set(ats_wnd on, char **data, int datasize);

C_API ats_wnd ats_tabwindow_set(ats_wnd on, char *tabmessage, _platform_cb tabfunc);
C_API void ats_tabinsert(ats_wnd tab, char *title, int slot);
C_API ats_wnd ats_image_set(ats_wnd on, char *pixmap);
C_API void ats_background_set(ats_wnd on, char *to_color);
C_API void ats_foreground_set(ats_wnd on, char *to_color);

C_API ats_wnd ats_button_set(ats_t *ats, ats_wnd on, ats_wnd alignto, const char *label,
	_platform_cb action, int is_vert);

C_API int ats_window(ats_t *ui, const char *title, int width, int height, int alloc_buffer);
C_API int ats_menubar_set(ats_t *ui, int numof_menus);
C_API int ats_font_set(ats_t *ui, const char *font);
C_API int ats_menu_set(ats_t *ui, int num_menu, menuitem_t *items, int number_items, int menu_id, char *name);
C_API void ats_dragdrop_set(ats_t *ui, const char *mime, int slot, _platform_cb dropfunc, _platform_cb dragfunc);
C_API void ats_dragdrop_update(ats_t *ui, const char *mime, int slot);
C_API void ats_title_set(ats_t *ui, const char *title);
C_API void ats_icon_set(ats_t *ui, const char *filepath);
C_API ats_wnd ats_windowgrid_set(ats_t *ui, int topheight, int bottomheight);
C_API ats_wnd ats_tabs_set(ats_wnd on);
C_API ats_wnd ats_boxwindow_set(ats_wnd on);
C_API ats_wnd ats_formwindow_set(ats_wnd on);
C_API int ats_graphics_loop(ats_t *ui);
C_API int ats_handler(ats_t *ui);
C_API void ats_close(ats_t *ui);

C_API void ats_editfile(ats_wnd, XtPointer, XtPointer);
C_API void ats_save_dialog(ui_t *, _platform_cb open_handler);
C_API void ats_open_dialog(ui_t *, _platform_cb save_handler);

C_API void ats_sleep(int64_t ms);
C_API int64_t ats_time(void);

#define ats_pixel(w, x, y) ((w)->buf[((y) * (w)->width) + (x)])

C_API int ats_webview(ats_t *ui, const char *title, const char *url,
	int width, int height, bool showtoolbar);
C_API void ats_webactive(ats_t *ui);
C_API void ats_webdestroy(ats_t *ui);

C_API size_t str_length(ui_form_t field);
C_API ui_bool str_field_valid(ui_form_t field, ui_field form);
C_API ui_bool str_is_regex(const char *pattern, ui_str_t match);
C_API ui_bool is_ValidUrl(ui_str_t text);
C_API ui_bool is_ValidEmail(ui_str_t text);
C_API ui_bool is_ValidPassword(ui_form_t field);

#define DEFAULT_URL                                                            \
  "data:text/"                                                                 \
  "html,%3C%21DOCTYPE%20html%3E%0A%3Chtml%20lang=%22en%22%3E%0A%3Chead%3E%"    \
  "3Cmeta%20charset=%22utf-8%22%3E%3Cmeta%20http-equiv=%22X-UA-Compatible%22%" \
  "20content=%22IE=edge%22%3E%3C%2Fhead%3E%0A%3Cbody%3E%3Cdiv%20id=%22app%22%" \
  "3E%3C%2Fdiv%3E%3Cscript%20type=%22text%2Fjavascript%22%3E%3C%2Fscript%3E%"  \
  "3C%2Fbody%3E%0A%3C%2Fhtml%3E"

#define CSS_INJECT_FUNCTION                                                    \
  "(function(e){var "                                                          \
  "t=document.createElement('style'),d=document.head||document."               \
  "getElementsByTagName('head')[0];t.setAttribute('type','text/"               \
  "css'),t.styleSheet?t.styleSheet.cssText=e:t.appendChild(document."          \
  "createTextNode(e)),d.appendChild(t)})"
#define WEBVIEW_API C_API

/*
 * MIT License
 *
 * For https://github.com/zserge/webview
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
enum webview_dialog_type {
	WEBVIEW_DIALOG_TYPE_OPEN = 0,
	WEBVIEW_DIALOG_TYPE_SAVE = 1,
	WEBVIEW_DIALOG_TYPE_ALERT = 2
};

enum webview_dialog_flag {
	WEBVIEW_DIALOG_FLAG_FILE = (0 << 0),
	WEBVIEW_DIALOG_FLAG_DIRECTORY = (1 << 0),

	WEBVIEW_DIALOG_FLAG_INFO = (1 << 1),
	WEBVIEW_DIALOG_FLAG_WARNING = (2 << 1),
	WEBVIEW_DIALOG_FLAG_ERROR = (3 << 1),
	WEBVIEW_DIALOG_FLAG_ALERT_MASK = (3 << 1),
};

enum {
	WEBVIEW_ID_ATS_BACK = ID_ATS_RESET + 1,
	WEBVIEW_ID_ATS_FORWARD,
	WEBVIEW_ID_ATS_HOME,
	WEBVIEW_ID_ATS_GOTO,
	WEBVIEW_ID_ATS_FIELD,
	WEBVIEW_ID_ATS_RELOAD,
};

typedef void (*webview_dispatch_fn)(webview_t *w, void *arg);
typedef struct webview_dispatch_arg webview_dispatch_arg;

struct webview_dispatch_arg {
	webview_dispatch_fn fn;
	webview_t *w;
	void *arg;
};

WEBVIEW_API int webview_run(const char *title, const char *url, int width, int height);

WEBVIEW_API int webview_loop(webview_t *w, int blocking);
WEBVIEW_API int webview_eval(webview_t *w, const char *js);
WEBVIEW_API int webview_inject_css(webview_t *w, const char *css);
WEBVIEW_API void webview_set_fullscreen(webview_t *w, int fullscreen);
WEBVIEW_API void webview_set_color(webview_t *w, uint8_t r, uint8_t g,
	uint8_t b, uint8_t a);
WEBVIEW_API void webview_dialog(webview_t *w,
	enum webview_dialog_type dlgtype, int flags,
	const char *title, const char *arg,
	char *result, size_t resultsz);
WEBVIEW_API void webview_dispatch(webview_t *w, webview_dispatch_fn fn,
	void *arg);
WEBVIEW_API void webview_exit(webview_t *w);
WEBVIEW_API void webview_debug(const char *format, ...);
WEBVIEW_API void webview_print_log(const char *s);

WEBVIEW_API int webview_create(ats_t *ui, webview_t *w);
WEBVIEW_API void webview_loadfile(webview_t *w, const char *resourcefile, const char *type);

/* Returns a native window handle pointer. */
WEBVIEW_API ats_wnd webview_get_window(webview_t *w);

/* Updates the title of the native window. Must be called from the UI thread. */
WEBVIEW_API void webview_set_title(webview_t *w, const char *title);

// Window size hints
#define WEBVIEW_HINT_NONE 0  // Width and height are default size
#define WEBVIEW_HINT_MIN 1   // Width and height are minimum bounds
#define WEBVIEW_HINT_MAX 2   // Width and height are maximum bounds
#define WEBVIEW_HINT_FIXED 3 // Window size can not be changed by a user

/* Updates native window size. See WEBVIEW_HINT constants. */
WEBVIEW_API void webview_set_size(webview_t *w, int width, int height, int hints);

/* Navigates webview to the given URL. URL may be a properly encoded data URI.
 *
 * Examples:
 * `webview_navigate(w, "https://github.com/webview/webview");`
 * `webview_navigate(w, "data:text/html,%3Ch1%3EHello%3C%2Fh1%3E");`
 * `webview_navigate(w, "data:text/html;base64,PGgxPkhlbGxvPC9oMT4=");` */
WEBVIEW_API void webview_navigate(webview_t *w, const char *url);

/* Set webview HTML directly.
 *
 * Example: `webview_set_html(w, "<h1>Hello</h1>");` */
WEBVIEW_API void webview_set_html(webview_t *w, const char *url);

WEBVIEW_API void webview_go_to(__ATS_WEBVIEW__);

// Go home
WEBVIEW_API void webview_home(__ATS_WEBVIEW__);

// Go back
WEBVIEW_API void webview_go_back(__ATS_WEBVIEW__);

// Go forward
WEBVIEW_API void webview_go_forward(__ATS_WEBVIEW__);

// Reload page
WEBVIEW_API void webview_reload(__ATS_WEBVIEW__);

// Stop loading page
WEBVIEW_API void webview_stop(__ATS_WEBVIEW__);

// Get current page title
WEBVIEW_API char *webview_get_title(webview_t *w);

// Get current page URL
WEBVIEW_API char *webview_get_url(webview_t *w);
//#endif /* _ATS_H */

#ifdef __cplusplus
}
#endif
//#endif

#include <tls.h>
#endif /* _ATHENA_H */