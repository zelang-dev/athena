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

	XtVaSetValues(di->dialog, XtNvalue, "", NULL);
	XtVaSetValues(di->ok, XtNsensitive, True, NULL);
	di->okd = false;
}

/*
 * DownloadDSave
 */
static void DownloadDSave(Widget w, XtPointer cldata, XtPointer cbdata) {
	DownloadInfo *di = (DownloadInfo *)cldata;
	ats_display_top(di->pop, di->cres->ats->topLevel, di->pop, true);
	DownloadDestroy(di);
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

	if ((filename = XawDialogGetValueString(di->dialog)) == NULL) {
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
		XtVaSetValues(di->dialog, XtNvalue, di->filename, NULL);
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
	di->pop = ats_modalwindow_set(di->cres->ats->wnd, "download");
	di->dialog = ats_dialogwindow_set(di->pop, "dialog", ATHENA_COLOR, "Enter Filename", di->filename);
	di->ok = XawDialogAddButton(di->dialog, "ok", DownloadOSave, di);
	XawDialogAddButton(di->dialog, "clear", DownloadClear, di);
	XawDialogAddButton(di->dialog, "dismiss", DownloadDSave, di);
	XtRealizeWidget(di->pop);
	ats_display_top(di->pop, di->cres->ats->topLevel, di->pop, false);
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

	if (di->dialog != NULL) {
		XtVaSetValues(di->dialog, XtNlabel, message, NULL);
	}
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
}
