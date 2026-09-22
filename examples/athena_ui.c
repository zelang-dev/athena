#include <Athena.h>

static Widget animator, canvas, color, image;
static Widget richtext, rudegrid, ruler, slider, vslider;
static Widget spinner, tabbing, table, textfield, tooltip;

static XColor xcolor;

static String fallback[] = {
	"*background: grey",
	"*MwRichtext.background: white",
	"*MwTextField.background: white",
	"*borderWidth: 0",
	"*Frame.shadowWidth: 1",
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

static void cb_open(ui_t *self, void *data) {
	ats_open_dialog(self, (_platform_cb)data);
}

static void cb_quit(ui_t *self, void *data) {
	ats_cancel(self->self);
}

static void cb_error(ui_t *self, void *data) {
	MwErrorBox(self->self, "Nothing to see here.");
}

static void cb_alert(ui_t *self, void *data) {
	char *buttons[] = {"Abort", "Retry", "Continue", "Fail"};
	MwAlertBox(self->self, "An unexpected error didn't occur at all.", buttons, 4);
}

static void cb_about(ui_t *self, void *data) {
	printf("cb_about(%s)\n", XtName(self->ats->wnd));
	MwAboutAthena(self->ats->topLevel);
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

static void CommandCB(Widget cmd, XtPointer client, XtPointer data) {
	ats_t *ui = (ats_t *)client;
	Widget *yesFrames = (Widget *)ui->app->app_array;
	Widget yesBut = yesFrames[1], yesFrame = yesFrames[0], noFrame = yesFrames[2];
	if (cmd == yesBut) {
		XtVaSetValues(yesFrame, XtNshadowType, Lowered, 0);
		XtVaSetValues(noFrame, XtNshadowType, Blank, 0);
	} else {
		XtVaSetValues(yesFrame, XtNshadowType, Blank, 0);
		XtVaSetValues(noFrame, XtNshadowType, Lowered, 0);
	}
}

int main(int argc, char **argv) {
	int i, error = -1;
	ats_wnd yesNoFrames[3], yesBut, yesFrame, noFrame;
	ats_t ui = {0};
	if (ats_window(&ui, "Athena UI", 600, 400, false)
		&& ats_menubar_set(&ui, 3)) {
		ats_dragdrop_set(&ui, "text/plain", 0, cb_drop, cb_drag);
		ats_dragdrop_update(&ui, "text/uri-list", 1);

		menuitem_t items[] = {
			{110, "Open", (_menu_cb)cb_open, "o", NULL},
			{__ATS_SEPERATOR__},
			{111, "Quit", cb_quit, "q", NULL},
		};

		menuitem_t items_two[] = {
			{112, "Alert", cb_alert, "A", NULL},
			{113, "Error", cb_error, "E", NULL},
		};

		menuitem_t items_three[] = {
			{114, "Contents", NULL, "C", NULL},
			{__ATS_SEPERATOR__},
			{115, "About", cb_about, "I", NULL},
		};

		if (!ats_font_set(&ui, helvetica)
			|| !ats_menu_set(&ui, 0, items, 3, 1, "File")
			|| !ats_menu_set(&ui, 1, items_two, 2, 2, "Misc")
			|| !ats_menu_set(&ui, 2, items_three, 3, 3, "Help")) {
			error = -2;
		}

		if (error == -1) {
			//ats_gridthree_set(ui.topLevel, 10, 10);
			/*
			ats_wnd tabs = ats_tabs_set(ui.topLevel);

			ats_wnd checkbox = ats_boxwindow_set(tabs);
			ats_checkradio_set(checkbox, "Check!", false);
			ats_checkradio_set(checkbox, "Radio!", true);
			ats_checkradio_set(checkbox, "Radio!", true);
			ats_checkradio_set(checkbox, "Check!", false);
			ats_label_set(checkbox, "Check/Radio");

			ats_wnd combobox = ats_boxwindow_set(tabs);
			ats_wnd combo = ats_combo_set(combobox, years, ats_sizeof(years));
			combo = ats_combo_set(combobox, days, ats_sizeof(days));
			combo = ats_combo_set(combobox, months, ats_sizeof(months));
			ats_label_set(combobox, "Combo");

			ats_wnd frameform = ats_formwindow_set(tabs);
			ats_wnd frame1 = ats_frame_set(frameform, Blank, 3);
			ats_label_set(frame1, "Blank");
			frame1 = ats_frame_set(frameform, Solid, 3);
			ats_label_set(frame1, "Solid");
			frame1 = ats_frame_set(frameform, Raised, 3);
			ats_label_set(frame1, "Raised");
			frame1 = ats_frame_set(frameform, Lowered, 3);
			ats_label_set(frame1, "Lowered");
			frame1 = ats_frame_set(frameform, Ridge, 3);
			ats_label_set(frame1, "Ridge");
			frame1 = ats_frame_set(frameform, Groove, 3);
			ats_label_set(frame1, "Groove");
			frame1 = ats_frame_set(frameform, Plateau, 3);
			ats_label_set(frame1, "Plateau");
			frame1 = ats_frame_set(frameform, Trough, 3);
			ats_label_set(frame1, "Trough");

			frame1 = ats_frame_set(frameform, Ridge, 3);
			frame1 = ats_label_set(frame1, "color");
			ats_background_set(frame1, "maroon");

			frame1 = ats_frame_set(frameform, Solid, 3);
			ats_frame_set(frame1, Plateau, 3);
			ats_frame_set(frame1, Lowered, 3);
			ats_label_set(frame1, "Nested");

			ui.app->app_array = (void **)yesNoFrames;
			yesFrame = ats_frame_set(frameform, Lowered, 1);
			yesNoFrames[0] = yesFrame;
			yesNoFrames[1] = ats_button_set(&ui, yesFrame, frame1, "Yes", CommandCB, true);
			noFrame = ats_frame_set(frameform, Blank, 1);
			yesNoFrames[2] = noFrame;
			ats_button_set(&ui, noFrame, yesFrame, "No", CommandCB, false);
			ats_label_set(frameform, "Frame");

			ats_wnd box = ats_boxwindow_set(tabs);
			image = ats_image_set(box, "netscape.xpm");
			ats_tooltip_set(&ui, image, "tool tip on image");
			ats_label_set(box, "Image");

			box = ats_boxwindow_set(tabs);
			textfield = ats_field_set(box, image, "type something", 0, 8, 300, field_secret, &ui);
			ats_tooltip_set(&ui, textfield, "tool tip on text field");
			ats_label_set(box, "Text field");

			box = ats_gridfull_set(tabs, 20);
			frame1 = ats_tabwindow_set(box, "Click tab to rename", select_tab);
			ats_tabinsert(frame1, "Page 1", 1);
			ats_tabinsert(frame1, "Page 2", 2);
			ats_label_set(box, "tabbing");
*/
			error = ats_handler(&ui);
		}

		ats_close(&ui);
	}

	return error;
}
