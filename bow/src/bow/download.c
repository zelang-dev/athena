/*
 * download.c
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>


#include "MyDialog.h"

#include "BowP.h"

typedef struct {
	MemPool mp;
	BowSink wp;
	Widget pop, dialog;
	bool okd;
	char *filename;
	bool ended;
	Widget ok;
	BowTask wt;
	BowResources cres;
} DownloadInfo;

static void DoSaveTask _ArgProto((DownloadInfo *));
static void SaveTask _ArgProto((void *));
static void DownloadDestroy _ArgProto((DownloadInfo *));
static void DownloadMakePop _ArgProto((DownloadInfo *));
static int DownloadInit _ArgProto((BowSink, void *));
static void DownloadAdd _ArgProto((void *));
static void DownloadEnd _ArgProto((void *));
static void DownloadMessage _ArgProto((void *, char *));

/*
 * DownloadDestroy
 */
static void DownloadDestroy(DownloadInfo *di) {
	BowResources cres = di->cres;

	if (di->pop != NULL) XtDestroyWidget(di->pop);
	if (di->wp != NULL) SinkDestroy(di->wp);
	MPDestroy(di->mp);

	BowRemoveReference(cres);
}

/*
 * DownloadClear
 */
static void DownloadClear(Widget w, XtPointer cldata, XtPointer cbdata) {
	DownloadInfo *di = (DownloadInfo *)cldata;

	MyDialogSetValue(di->dialog, "");
	XtVaSetValues(di->ok, XtNsensitive, True, NULL);
	di->okd = false;
}

/*
 * DownloadDSave
 */
static void DownloadDSave(Widget w, XtPointer cldata, XtPointer cbdata) {
	DownloadDestroy((DownloadInfo *)cldata);
}

/*
 * SaveTask
 */
static void SaveTask(void *closure) {
	char *filename;
	byte *data;
	size_t len;
	MIMEHeader mh;
	FILE *fp;
	DownloadInfo *di = (DownloadInfo *)closure;

	if ((filename = MyDialogGetValue(di->dialog)) == NULL) {
		XBell(di->cres->dpy, 100);
		XtVaSetValues(di->ok, XtNsensitive, True, NULL);
		di->okd = false;
		di->wt = NULL;
		return;
	}

	filename = FixPath(di->mp, filename);
	if ((fp = fopen(filename, "w")) == NULL) {
		XBell(di->cres->dpy, 100);
		XtVaSetValues(di->ok, XtNsensitive, True, NULL);
		di->okd = false;
		di->wt = NULL;
		MyDialogSetValue(di->dialog, di->filename);
		return;
	}

	SinkGetData(di->wp, &data, &len, &mh);

	fwrite(data, 1, len, fp);
	fclose(fp);

	DownloadDestroy(di);
}

/*
 * DoSaveTask
 */
static void DoSaveTask(DownloadInfo *di) {
	myassert(di->wt == NULL, "Save task already started.");

	di->wt = TaskSchedule(di->cres, SaveTask, di);
}

/*
 * DownloadOSave
 */
static void DownloadOSave(Widget w, XtPointer cldata, XtPointer cbdata) {
	DownloadInfo *di = (DownloadInfo *)cldata;

	if (di->okd) return;

	if (di->ended) DoSaveTask(di);
	else {
		di->okd = true;
		XtVaSetValues(di->ok, XtNsensitive, False, NULL);
	}
}

static void DownloadMakePop(DownloadInfo *di) {
	Window rw, cw;
	int rx, ry, wx, wy;
	unsigned int mask;

	XQueryPointer(di->cres->dpy, DefaultRootWindow(di->cres->dpy),
		&rw, &cw,
		&rx, &ry,
		&wx, &wy,
		&mask);
	di->pop = XtVaAppCreateShell("download", "Download",
		transientShellWidgetClass, di->cres->dpy,
		XtNx, rx - 2,
		XtNy, ry - 2,
		NULL);
	di->dialog = XtVaCreateManagedWidget("dialog",
		mydialogWidgetClass, di->pop,
		NULL);
	di->ok = MyDialogAddButton(di->dialog, "ok", DownloadOSave, di);
	MyDialogAddButton(di->dialog, "Clear", DownloadClear, di);
	MyDialogAddButton(di->dialog, "dismiss", DownloadDSave, di);
	XtAddCallback(di->dialog, XtNcallback, DownloadOSave, di);

	XtRealizeWidget(di->pop);

	MyDialogSetValue(di->dialog, di->filename);
}

/*
 * DownloadAdd
 */
void DownloadAdd(void *closure) {
}

/*
 * DownloadEnd
 */
void DownloadEnd(void *closure) {
	DownloadInfo *di = (DownloadInfo *)closure;

	di->ended = true;
	if (di->okd) DoSaveTask(di);
}

/*
 * DownloadMessage
 */
void DownloadMessage(void *closure, char *message) {
	DownloadInfo *di = (DownloadInfo *)closure;

	if (di->dialog != NULL)
		MyDialogSetMessage(di->dialog, message);
}

/*
 * DownloadInit
 */
static int DownloadInit(BowSink wp, void *closure) {
	return(0);
}

void DownloadOpen(BowResources cres, BowRequest *wr) {
	DownloadInfo *di;
	MemPool mp;
	BowSinkHooks hooks;
	char *filename;

	myassert(wr != NULL, "NULL request not allowed.");

	mp = MPCreate();
	di = (DownloadInfo *)MPCGet(mp, sizeof(DownloadInfo));
	di->mp = mp;
	if ((filename = GetBaseFilename(wr->url)) == NULL) filename = "";
	di->filename = MPStrDup(di->mp, filename);
	di->cres = cres;

	memset(&hooks, 0, sizeof(hooks));
	hooks.init = DownloadInit;
	hooks.add = DownloadAdd;
	hooks.end = DownloadEnd;
	hooks.message = DownloadMessage;

	BowAddReference(cres);

	if ((di->wp = SinkCreate(cres, wr)) != NULL) {
		SinkSetHooks(di->wp, &hooks, di);
		DownloadMakePop(di);
	} else {
		DownloadDestroy(di);
		RequestDestroy(wr);
	}

	MwSetIcon(di->pop, icon_32x32);
}
