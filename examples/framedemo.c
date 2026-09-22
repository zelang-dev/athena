#include <Athena.h>

	/* X11 stuff */

static	Display *dpy;
static	Atom	wm_protocols;
static	Atom	wm_delete_window;

	/* Xt stuff */

static	XtAppContext	app_ctx;

static	Widget	topLevel, yesFrame, noFrame, yesBut, noBut;

static String fallback[] = {

	"*background: silver",
	"Framedemo.baseTranslations: <Message>WM_PROTOCOLS: WMProtocols()",

	"*borderWidth: 0",
	"*marginframe.shadowWidth: 0",
	"*marginframe.shadowType: blank",
	"*marginframe.Margin: 10",
	"*mainframe.label: Frame label",
	"*mainframe.shadowWidth: 2",
	"*mainframe.shadowType: groove",
	"*mainframe.Margin: 4",
	"*maintitle.label: Main Title",
	"*frame1.shadowType: solid",
	"*frame1.lbl.label: solid",
	"*frame2.shadowType: raised",
	"*frame2.lbl.label: raised",
	"*frame2.shadowWidth: 1",
	"*frame2.fromHoriz: frame1",
	"*frame3.shadowType: lowered",
	"*frame3.lbl.label: lowered",
	"*frame3.shadowWidth: 1",
	"*frame3.fromHoriz: frame2",
	"*frame4.shadowType: blank",
	"*frame4.lbl.label: blank",
	"*frame4.fromHoriz: frame3",
	"*frame5.shadowType: groove",
	"*frame5.lbl.label: groove",
	"*frame5.fromVert: frame1",
	"*frame6.shadowType: ridge",
	"*frame6.lbl.label: ridge",
	"*frame6.fromVert: frame1",
	"*frame6.fromHoriz: frame5",
	"*frame7.shadowType: plateau",
	"*frame7.lbl.label: plateau",
	"*frame7.shadowWidth: 6",
	"*frame7.fromVert: frame1",
	"*frame7.fromHoriz: frame6",
	"*frame8.shadowType: trough",
	"*frame8.lbl.label: trough",
	"*frame8.shadowWidth: 6",
	"*frame8.fromHoriz: frame7",
	"*frame8.fromVert: frame1",
	"*frame9.shadowType: ridge",
	"*frame9.lbl.label: color",
	"*frame9.shadowWidth: 4",
	"*frame9.fromVert: frame7",
	"*frame9*background: maroon",
	"*framen.shadowType: ridge",
	"*framen.shadowWidth: 2",
	"*framen2.shadowType: ridge",
	"*framen2.shadowWidth: 2",
	"*framen3.shadowType: ridge",
	"*framen3.shadowWidth: 2",
	"*framen*Margin: 2",
	"*framen*lbl.label: nested",
	"*framen.fromHoriz: frame9",
	"*framen.fromVert: frame7",
	"*frame10.shadowType: lowered",
	"*frame10.btn.label: yes",
	"*frame10.shadowWidth: 1",
	"*frame10.Margin: 4",
	"*frame10.fromVert: framen",
	"*frame11.shadowType: blank",
	"*frame11.btn.label: no",
	"*frame11.shadowWidth: 0",
	"*frame11.Margin: 4",
	"*frame11.fromVert: framen",
	"*frame11.fromHoriz: frame10",

	NULL
};

static	void	FrameDemo();

static	void	WMProtocols(Widget, XEvent *, String *, Cardinal *);

static	void	CommandCB(Widget cmd, XtPointer client, XtPointer data);


	/* define global actions table for the application.  This is done
	 * to catch messages.  This could also be done through the main
	 * event loop if we wrote one.  It would be nice if Xt had a way
	 * of regestering a callback proc for specific kinds of events.
	 */

XtActionsRec	fd_actions[] = {
	{"WMProtocols", WMProtocols},
};

int main(int argc, char **argv) {
	topLevel = XtAppInitialize(&app_ctx, "Framedemo", NULL, 0,
		&argc, argv, fallback, NULL, 0);
	dpy = XtDisplay(topLevel);

	XtAppAddActions(app_ctx, fd_actions, XtNumber(fd_actions));

	FrameDemo();

	XtRealizeWidget(topLevel);

	/* tell window system we're willing to handle window-delete messages */
	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	(void)XSetWMProtocols(dpy, XtWindow(topLevel), &wm_delete_window, 1);

	XtAppMainLoop(app_ctx);

	exit(0);
	/* NOTREACHED */
}



static	void
FrameDemo() {
	Widget	marginFrame, mainFrame;
	Widget	form;
	Widget	fm, lbl;

	marginFrame = XtVaCreateManagedWidget("marginframe", mwFrameWidgetClass,
		topLevel, 0);

	mainFrame = XtVaCreateManagedWidget("mainframe", mwFrameWidgetClass,
		marginFrame, 0);
	lbl = XtCreateManagedWidget("maintitle", labelWidgetClass, mainFrame,
		NULL, 0);
	XtVaSetValues(mainFrame, XtNtitle, lbl, 0);

	form = XtVaCreateManagedWidget("form", formWidgetClass,
		mainFrame, 0);

	fm = XtVaCreateManagedWidget("frame1", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame2", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame3", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame4", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame5", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame6", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame7", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame8", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("frame9", mwFrameWidgetClass, form, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	fm = XtVaCreateManagedWidget("framen", mwFrameWidgetClass, form, 0);
	fm = XtVaCreateManagedWidget("framen2", mwFrameWidgetClass, fm, 0);
	fm = XtVaCreateManagedWidget("framen3", mwFrameWidgetClass, fm, 0);
	lbl = XtCreateManagedWidget("lbl", labelWidgetClass, fm, NULL, 0);

	yesFrame =
		XtVaCreateManagedWidget("frame10", mwFrameWidgetClass, form, 0);
	yesBut =
		XtCreateManagedWidget("Yes", commandWidgetClass, yesFrame, NULL, 0);
	XtAddCallback(yesBut, XtNcallback, CommandCB, NULL);

	noFrame =
		XtVaCreateManagedWidget("frame11", mwFrameWidgetClass, form, 0);
	noBut =
		XtCreateManagedWidget("No", commandWidgetClass, noFrame, NULL, 0);
	XtAddCallback(noBut, XtNcallback, CommandCB, NULL);
}





/* ARGSUSED */
static	void
WMProtocols(w, ev, params, nparams)
Widget	w;
XEvent *ev;
String *params;
Cardinal *nparams;
{
	if (ev->type == ClientMessage &&
		ev->xclient.message_type == wm_protocols &&
		ev->xclient.data.l[0] == wm_delete_window) {
		if (w == topLevel)
			exit(0);
	}
}


static	void CommandCB(Widget cmd, XtPointer client, XtPointer data) {
	if (cmd == yesBut) {
		XtVaSetValues(yesFrame, XtNshadowType, Lowered, 0);
		XtVaSetValues(noFrame, XtNshadowType, Blank, 0);
	} else {
		XtVaSetValues(yesFrame, XtNshadowType, Blank, 0);
		XtVaSetValues(noFrame, XtNshadowType, Lowered, 0);
	}
}
