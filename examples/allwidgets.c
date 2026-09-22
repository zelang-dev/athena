#include <Athena.h>

#define MENUBAR (1)

static int bars = MENUBAR;

static Widget topLevel, topbox;
static Widget animator, canvas, check, color, combo, frame, image;
static Widget menubar, menubox, menubutton, menu;
static Widget richtext, rudegrid, ruler, slider, vslider;
static Widget spinner, tabbing, table, tabs, textfield, tooltip;

static XColor xcolor;

static String fallback[] = {
	"*background: silver",
	"*Frame.shadowWidth: 9",
	"*Frame.shadowType: blank",
	"*font: -*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-*",
	NULL
};

static char *years[] = {
	"2000", "2001", "2002", "2003", "2004", "2005", "2006",
	"2007", "2008", "2009", "2010", "2011", "2012", "2013",
	"2014", "2015", "2016", "2017", "2018", "2019", "2020"
};

static char *months[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

static char *days[] = {
	"Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday", "Sunday"
};

static MwAniScript script0[] = {
	{0, 0, 0, 10, 100, 1, script0 + 1},
	{1000, 0, 0, 100, 10, 1, NULL}};

static MwAniScript script1[] = {{0, 110, 20, 0, 0, 1, NULL}};
static MwAniScript script2[] = {{0, 110, 35, 0, 0, 1, NULL}};
static MwAniScript script3[] = {{0, 110, 50, 0, 0, 1, NULL}};
static MwAniScript script4[] = {{0, 110, 65, 0, 0, 1, NULL}};

static MwAniObject cast[] = {
	{MW_ANI_RECTANGLE, "Object0", script0, 0, "", cast + 1},
	{MW_ANI_STRING, "Object1", script1, 0, "C = play", cast + 2},
	{MW_ANI_STRING, "Object2", script2, 0, "S = pause", cast + 3},
	{MW_ANI_STRING, "Object3", script3, 0, "P = previous frame", cast + 4},
	{MW_ANI_STRING, "Object4", script4, 0, "N = next frame", NULL},
};

static MwFmt myfmt = {"Courier", 150, 1, 0, 0, 0, "red", "yellow", 0,
		MW_VADJ_CENTER, MW_HADJ_LEFT, 0};

/* width height colours a z colour1 colour2 */
static char *mygrad = "100 100 128 0 1 SpringGreen Chartreuse";

static void cb_drag(Widget w, XtPointer client_data, XtPointer call_data) {
	DropPosition *where = (DropPosition *)call_data;
	printf("cb_drag at (%d,%d)\n", where->x, where->y);
}

static void cb_drop(Widget w, XtPointer client_data, XtPointer call_data) {
	intptr_t prot = (intptr_t)call_data;
	int dnd_type;
	Atom xdnd_type;
	char *data;

	switch (prot) {
		case DndDrop:
			printf("cb_drop received OffiX drop\n");
			XtVaGetValues(w,
				XtNdndType, &dnd_type,
				XtNdropData, &data,
				NULL);
			printf("type = %d, data = '%s'\n", dnd_type, data);
			break;
		case XdndDrop:
			printf("cb_drop received Xdnd drop\n");
			XtVaGetValues(w,
				XtNxdndType, &xdnd_type,
				XtNdropData, &data,
				NULL);
#if 1
			printf("type = '%s', data = '%s'\n",
				XGetAtomName(XtDisplay(w), xdnd_type), data);
#else
			printf("type = %ld, data = '%s'\n",
				xdnd_type, data);
#endif
			break;
		case MotifDrop:
			printf("cb_drop received Motif drop\n");
			break;
		default:
			printf("cb_drop received unknown drop\n");
			break;
	}
}

static void redraw_canvas(Widget w, XtPointer client_data, XtPointer call_data) {
	printf("redraw canvas\n");
}

static void cb_spinner(Widget w, XtPointer client_data, XtPointer call_data) {
	intptr_t n = (intptr_t)call_data;
	printf("cb_spinner(%s) => %d\n", XtName(w), n);
}

static void set_color(XColor *xcolor) {
	MwAllocColor(XtDisplay(color), None, xcolor);
	XtVaSetValues(color, XtNbackground, xcolor->pixel, NULL);
}

static void cb_red(Widget w, XtPointer client_data, XtPointer call_data) {
	intptr_t n = (intptr_t)call_data;
	xcolor.red = n;
	set_color(&xcolor);
}

static void cb_green(Widget w, XtPointer client_data, XtPointer call_data) {
	intptr_t n = (intptr_t)call_data;
	xcolor.green = n;
	set_color(&xcolor);
}

static void cb_blue(Widget w, XtPointer client_data, XtPointer call_data) {
	intptr_t n = (intptr_t)call_data;
	xcolor.blue = n;
	set_color(&xcolor);
}

static void cb_open(Widget w, XtPointer client_data, XtPointer call_data) {
	char path[1024] = ".";
	char name[1024] = "";
	char *patterns[] = {"All files (*)", NULL};
	char fmt[1024] = "";
	char filepath[FILENAME_MAX] = "";
	char *extra = NULL;
	int ext = 0;
	int s = MwFileselInput(topLevel, path, name, patterns, fmt, extra, ext);
	if (strlen(name) > 0 && s) {
		snprintf(filepath, sizeof(filepath), "%s/%s", path, name);
		if (call_data)
			((_platform_cb)(call_data))(w, NULL, filepath);
		else
			ats_editfile(w, NULL, filepath);
	}
}

static void cb_quit(Widget w, XtPointer client_data, XtPointer call_data) {
	MwMallocExit();
	exit(0);
}

static void cb_error(Widget w, XtPointer client_data, XtPointer call_data) {
	MwErrorBox(topLevel, "Nothing to see here.");
}


static void cb_alert(Widget w, XtPointer client_data, XtPointer call_data) {
	char *buttons[] = {"Abort", "Retry", "Continue", "Fail"};
	MwAlertBox(topLevel, "An unexpected error didn't occur at all.",
		buttons, 4);
}


static void cb_about(Widget w, XtPointer client_data, XtPointer call_data) {
	printf("cb_about(%s)\n", XtName(w));
	MwAboutAthena(topLevel);
}

static void quit(Widget w, XEvent *event, String *params, Cardinal *n) {
	exit(0);
}

static XtActionsRec actions[] =
{
	{"quit", quit}
};

static void remake_ylayout(void) {
	char b[100];
	sprintf(b, "%s 100%%",
		(bars & MENUBAR) ? "30" : "0");
	XtVaSetValues(rudegrid,
		XtNyLayout, b,
		NULL);
}

static void attach(Widget w, XtPointer client_data, XtPointer call_data) {
	Widget vw = (Widget)call_data;
	printf("attach(%s)\n", XtName(w));
	if (vw == menubar) bars |= MENUBAR;
	remake_ylayout();
}

static void detach(Widget w, XtPointer client_data, XtPointer call_data) {
	Widget vw = (Widget)call_data;
	printf("detach(%s)\n", XtName(w));
	if (vw == menubar) bars &= ~MENUBAR;
	remake_ylayout();
}

static void select_tab(Widget w, XtPointer client_data, XtPointer call_data) {
	Widget label = (Widget)client_data;
	intptr_t pos = (intptr_t)call_data;
	char *n = MwTabbingPosToText(w, pos);
	XtVaSetValues(label, XtNlabel, n, NULL);
}

static char *table_text(void *p, int r, int c) {
	static char b[1024];
	sprintf(b, "%d*%d = %d", r, c, r * c);
	return b;
}

int main(int argc, char **argv) {
	Atom wm_protocols, wm_delete_window;
	Atom *drop_types;
	XtAppContext ac;
	Widget w;
	int i;

	topLevel = XtVaOpenApplication(&ac, "AllWidgets",
		NULL, 0,
		&argc, argv,
		fallback,
		mwApplicationShellWidgetClass,
		XtNwidth, 300,
		XtNheight, 200,
		NULL);
	XtAppAddActions(ac, actions, XtNumber(actions));
	XawFocusInstallActions(ac);
	drop_types = MwMalloc(3 * sizeof * drop_types);
	drop_types[0] = XInternAtom(XtDisplay(topLevel), "text/plain", False);
	drop_types[1] = XInternAtom(XtDisplay(topLevel), "text/uri-list", False);
	drop_types[2] = None;
	XtVaSetValues(topLevel,
		XtNdropTypes, drop_types,
		NULL);
	XtAddCallback(topLevel, XtNdragCallback, cb_drag, NULL);
	XtAddCallback(topLevel, XtNdropCallback, cb_drop, NULL);
	char goo[60];
	snprintf(goo, sizeof(goo), "%d %s", 25, "100%");
	rudegrid = XtVaCreateManagedWidget("rudegrid",
		mwRudegridWidgetClass, topLevel,
		XtNborder, 0,
		XtNbackground, 0xffffff,
		XtNborderWidth, 0,
		XtNyLayout, goo,
		NULL);
	menubar = XtVaCreateManagedWidget("menubar",
		mwRudegridWidgetClass, rudegrid,
		XtNborderWidth, 0,
		XtNxLayout, "0 100%",
		NULL);

	MwInitFormat(XtDisplayOfObject(menubar));
	//MwMakeHandle(menubar, menubar, detach, attach);
	menubox = XtVaCreateManagedWidget("menubox",
		mwMenuBarWidgetClass, menubar,
		XtNborder, 0,
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		XtNgridx, 1,
		NULL);
	bars |= MENUBAR;
	menubutton = XtVaCreateManagedWidget("menubutton",
		mwMBButtonObjectClass, menubox,
		XtNlabel, "File",
		XtNborderWidth, 0,
		XtNborder, 0,
		XtNbackground, 0xffffff,
		XtNmenu_name, "filemenu",
		NULL);
	menu = XtVaCreatePopupShell("filemenu",
		mwMenuWidgetClass, menubox, NULL, 0);
	w = XtVaCreateManagedWidget("Open",
		mwLabelMEObjectClass, menu,
		XtNlabel, "Open",
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		NULL);
	XtAddCallback(w, XtNcallback, cb_open, NULL);
	XtVaCreateManagedWidget("-",
		mwLineMEObjectClass, menu,
		NULL);
	w = XtVaCreateManagedWidget("Exit",
		mwLabelMEObjectClass, menu,
		XtNlabel, "Exit",
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		NULL);
	XtAddCallback(w, XtNcallback, cb_quit, NULL);

	menubutton = XtVaCreateManagedWidget("menubutton",
		mwMBButtonObjectClass, menubox,
		XtNlabel, "Misc",
		XtNbackground, 0xffffff,
		XtNmenu_name, "miscmenu",
		XtNborderWidth, 0,
		NULL);
	menu = XtVaCreatePopupShell("miscmenu",
		mwMenuWidgetClass, menubox, NULL, 0);
	w = XtVaCreateManagedWidget("Alert",
		mwLabelMEObjectClass, menu,
		XtNlabel, "Alert",
		XtNbackground, 0xffffff,
		XtNborderWidth, 0,
		NULL);
	XtAddCallback(w, XtNcallback, cb_alert, NULL);
	w = XtVaCreateManagedWidget("Error",
		mwLabelMEObjectClass, menu,
		XtNlabel, "Error",
		XtNbackground, 0xffffff,
		XtNborderWidth, 0,
		XtNfont, XLoadQueryFont(XtDisplayOfObject(topLevel), "lucidasans-8"),
		NULL);
	XtAddCallback(w, XtNcallback, cb_error, NULL);

	menubutton = XtVaCreateManagedWidget("menubutton",
		mwMBButtonObjectClass, menubox,
		XtNlabel, "Help",
		XtNmenu_name, "helpmenu",
		XtNbackground, 0xffffff,
		NULL);
	menu = XtVaCreatePopupShell("helpmenu",
		mwMenuWidgetClass, menubox, NULL, 0);
	XtVaCreateManagedWidget("Contents",
		mwLabelMEObjectClass, menu,
		XtNbackground, 0xffffff,
		XtNlabel, "Contents",
		NULL);
	XtVaCreateManagedWidget("-",
		mwLineMEObjectClass, menu,
		NULL);
	w = XtVaCreateManagedWidget("About",
		mwLabelMEObjectClass, menu,
		XtNbackground, 0xffffff,
		XtNlabel, "About",
		NULL);
	XtAddCallback(w, XtNcallback, cb_about, NULL);
	tabs = XtVaCreateManagedWidget("tabs",
		mwTabsWidgetClass, rudegrid,
		XtNbackground, 0xffffff,
		XtNgridy, 1,
		NULL);

	for (i = 0; i < 5; i++) {
		cast[i].fmt = MwEncodeFormat(~0, &myfmt);
	}
	animator = XtVaCreateManagedWidget("animator",
		mwAnimatorWidgetClass, tabs,
		XtNanimatorCast, cast,
		XtNgradient, mygrad,
		XtNbackground, 0xffffff,
		NULL);

	canvas = XtVaCreateManagedWidget("canvas",
		mwCanvasWidgetClass, tabs,
		XtNbackground, 0xffffff,
		NULL);
	XtAddCallback(canvas, XtNcallback, redraw_canvas, NULL);
	topbox = XtVaCreateManagedWidget("check",
		boxWidgetClass, tabs,
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		NULL);
	check = XtVaCreateManagedWidget("check",
		mwCheckWidgetClass, topbox,
		XtNlabel, "Check!",
		XtNborderWidth, 0,
		NULL);
	check = XtVaCreateManagedWidget("check",
		mwCheckWidgetClass, topbox,
		XtNlabel, "Radio!",
		XtNborderWidth, 0,
		XtNcheckStyle, MwRadioWin,
		XtNradioGroup, topbox,
		XtNradioStart, topbox,
		NULL);
	check = XtVaCreateManagedWidget("check",
		mwCheckWidgetClass, topbox,
		XtNlabel, "Radio!",
		XtNborderWidth, 0,
		XtNcheckStyle, MwRadioWin,
		XtNradioGroup, topbox,
		XtNradioStart, topbox,
		NULL);
	check = XtVaCreateManagedWidget("check",
		mwCheckWidgetClass, topbox,
		XtNborderWidth, 0,
		XtNlabel, "Check!",
		NULL);
	color = XtVaCreateManagedWidget("color",
		mwRudegridWidgetClass, tabs,
		XtNxLayout, "4 80 4 80",
		XtNyLayout, "4 22 4 22 4 22",
		NULL);
	MwAllocNamedColor(XtDisplay(color), "red", &xcolor);
	XtVaCreateManagedWidget("Red",
		labelWidgetClass, color,
		XtNgridx, 1, XtNgridy, 1,
		XtNbackground, xcolor.pixel,
		NULL);
	w = XtVaCreateManagedWidget("red",
		mwSpinnerWidgetClass, color,
		XtNgridx, 3, XtNgridy, 1,
		XtNmin, 0, XtNmax, 65535, XtNstep, 100,
		NULL);
	XtAddCallback(w, XtNcallback, cb_red, NULL);
	MwAllocNamedColor(XtDisplay(color), "green", &xcolor);
	XtVaCreateManagedWidget("Green",
		labelWidgetClass, color,
		XtNgridx, 1, XtNgridy, 3,
		XtNbackground, xcolor.pixel,
		NULL);
	w = XtVaCreateManagedWidget("green",
		mwSpinnerWidgetClass, color,
		XtNgridx, 3, XtNgridy, 3,
		XtNmin, 0, XtNmax, 65535, XtNstep, 100,
		NULL);
	XtAddCallback(w, XtNcallback, cb_green, NULL);
	MwAllocNamedColor(XtDisplay(color), "blue", &xcolor);
	XtVaCreateManagedWidget("Blue",
		labelWidgetClass, color,
		XtNgridx, 1, XtNgridy, 5,
		XtNbackground, xcolor.pixel,
		NULL);
	w = XtVaCreateManagedWidget("blue",
		mwSpinnerWidgetClass, color,
		XtNgridx, 3, XtNgridy, 5,
		XtNmin, 0, XtNmax, 65535, XtNstep, 100,
		NULL);
	XtAddCallback(w, XtNcallback, cb_blue, NULL);
	xcolor.red = 0;
	xcolor.green = 0;
	xcolor.blue = 0;
	set_color(&xcolor);
	image = XtVaCreateManagedWidget("image",
		mwImageWidgetClass, tabs,
		NULL);
	richtext = XtVaCreateManagedWidget("richtext",
		boxWidgetClass, tabs,
		NULL);
	XtVaCreateManagedWidget("richtext",
		mwRichtextWidgetClass, richtext,
		XtNborderWidth, 0,
		NULL);
	XtVaCreateManagedWidget("richtext",
		mwRichtextWidgetClass, richtext,
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		NULL);
	richtext = XtVaCreateManagedWidget("richtext",
		mwRichtextWidgetClass, richtext,
		XtNborderWidth, 0,
		NULL);
	ruler = XtVaCreateManagedWidget("ruler",
		mwRulerWidgetClass, tabs,
		NULL);
	topbox = XtVaCreateManagedWidget("slider",
		boxWidgetClass, tabs,
		NULL);
	slider = XtVaCreateManagedWidget("slider",
		mwSliderWidgetClass, topbox,
		XtNborderWidth, 0,
		NULL);
	vslider = XtVaCreateManagedWidget("vslider",
		mwVSliderWidgetClass, topbox,
		XtNborderWidth, 0,
		NULL);
	topbox = XtVaCreateManagedWidget("spinner",
		boxWidgetClass, tabs,
		XtNborderWidth, 0,
		NULL);
	spinner = XtVaCreateManagedWidget("spinner",
		mwSpinnerWidgetClass, topbox,
		NULL);
	XtAddCallback(spinner, XtNcallback, cb_spinner, NULL);
	spinner = XtVaCreateManagedWidget("spinner",
		mwSpinnerWidgetClass, topbox,
		XtNmin, -100, XtNstep, 10, XtNmax, 100,
		XtNborderWidth, 0,
		NULL);
	XtAddCallback(spinner, XtNcallback, cb_spinner, NULL);
	spinner = XtVaCreateManagedWidget("spinner",
		mwSpinnerWidgetClass, topbox,
		XtNmin, -1000, XtNstep, 20, XtNmax, 0,
		NULL);
	XtAddCallback(spinner, XtNcallback, cb_spinner, NULL);
	spinner = XtVaCreateManagedWidget("spinner",
		mwSpinnerWidgetClass, topbox,
		XtNmin, 0, XtNstep, 5, XtNmax, 1000,
		NULL);
	XtAddCallback(spinner, XtNcallback, cb_spinner, NULL);
	XtVaSetValues(spinner, XtNvalue, 10, NULL);
	topbox = XtVaCreateManagedWidget("textfield",
		boxWidgetClass, tabs,
		NULL);
	textfield = XtVaCreateManagedWidget("textfield",
		mwTextfieldWidgetClass, topbox,
		XtNborderWidth, 0,
		XtNbackground, 0xffffff,
		NULL);
	topbox = XtVaCreateManagedWidget("tabbing",
		mwRudegridWidgetClass, tabs,
		XtNyLayout, "100% 20",
		XtNbackground, 0xffffff,
		NULL);
	w = XtVaCreateManagedWidget("tabbing",
		labelWidgetClass, topbox,
		XtNlabel, "Click tab to rename",
		XtNborderWidth, 0,
		NULL);
	tabbing = XtVaCreateManagedWidget("tabbing",
		mwTabbingWidgetClass, topbox,
		XtNgridy, 1,
		XtNborderWidth, 0,
		NULL);
	image = ats_image_set(topbox, "netscape.xpm");
	MwTabbingInsert(tabbing, "Page 1", 1);
	MwTabbingInsert(tabbing, "Page 2", 2);
	XtAddCallback(tabbing, XtNselectCallback, select_tab, (XtPointer)w);
	table = XtVaCreateManagedWidget("table",
		mwTableWidgetClass, tabs,
		XtNbackground, 0xffffff,
		XtNtableText, table_text,
		NULL);
	tooltip = XtVaCreatePopupShell("tooltip",
		mwTooltipWidgetClass, topLevel,
		NULL);
	MwTooltipAdd(tooltip, textfield, "Ulric fibbar");
	XtRealizeWidget(topLevel);
	wm_protocols = XInternAtom(XtDisplay(topLevel),
		"WM_PROTOCOLS", False);
	wm_delete_window = XInternAtom(XtDisplay(topLevel),
		"WM_DELETE_WINDOW", False);
	XtOverrideTranslations(topLevel,
		XtParseTranslationTable(
			"<Message>WM_PROTOCOLS: quit()"));
	XSetWMProtocols(XtDisplay(topLevel), XtWindow(topLevel),
		&wm_delete_window, 1);
	XtAppMainLoop(ac);
	return 0;
}
