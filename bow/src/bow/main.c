/*
 * main.c
 *
 * Copyright (C) 1993-1997, John Kilburg <john@cs.unlv.edu>
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
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <athena.h>

#include "MyDialog.h"

#include "BowP.h"

static void sigigh_handler _ArgProto(());
static void xtWarningHandler _ArgProto((String));
static int xErrorHandler _ArgProto((Display *, XErrorEvent *));
static void BowCleanup _ArgProto((BowResources));
static BowResources ResourcesCreate _ArgProto((int *, char **));
static void ResourcesDestroy _ArgProto((BowResources));

extern char *fallback_resources[];

static BowResources globalcres;
static char *default_resource = "bookmark.filename: ~/.bow/bookmarks.html\n"
"cache.directory: ~/.bow/cache\n"
"cache.persist: true\n"
"bow.homeURL: http://www.google.com\n"
"html.propFontPattern: -adobe-helvetica-*-*-*-*-*-*-*-*-*-*-iso8859-1\n"
"view.capFiles: ~/.bow/mailcap:~/.mailcap\n";

int main(int argc, char **argv) {
	char base_url[255];

	signal(SIGINT, sigigh_handler);
	signal(SIGQUIT, sigigh_handler);
	signal(SIGHUP, sigigh_handler);
	signal(SIGTERM, sigigh_handler);
	signal(SIGPIPE, SIG_IGN);

	StartReaper();

	globalcres = ResourcesCreate(&argc, argv);

	/*
	 * Default base URL; this allows filenames to be used on the
	 * command line
	 */
	strcpy(base_url, "file:");
	getcwd(base_url + 5, sizeof(base_url) - 5);
	strcat(base_url, "/");

	ats_t ui = {0};
	globalcres->ats = &ui;
	globalcres->ats->app_con = globalcres->appcon;
	globalcres->ats->dpy = globalcres->dpy;
	ats_athena_set(globalcres->ats, icon_32x32, "Browser", 650, 500);

	if (argc > 1)
		HeadCreate(globalcres, RequestCreate(globalcres, argv[argc - 1], base_url), NULL);
	else
		HeadCreate(globalcres, NULL, NULL);

	/*
	 * And away we go...
	 */
	ats_handler(globalcres->ats);
	ats_close(globalcres->ats);
	return 0;
}

/*
 * sigiqh_handler
 */
static void sigigh_handler() {
}

/*
 * xErrorHandler
 */
 int xErrorHandler(Display *dpy, XErrorEvent *xe) {
	fprintf(stderr, "X error\n");
	fflush(stderr);
	return 0;
}

/*
 * xtErrorHandler
 */
 static _X_NORETURN void xtErrorHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
 }

/*
 * xtWarningHandler
 */
static void xtWarningHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
}

/*
 * BowAddReference
 */
void BowAddReference(BowResources cres) {
	cres->refcount++;
	return;
}

/*
 * BowCleanup
 */
static void BowCleanup(BowResources cres) {
	if (cres->bc != NULL) BookmarkDestroyContext(cres->bc);

	ResourcesDestroy(cres);

	if (cres->logfp != NULL) fclose(cres->logfp);

	GListDestroy(cres->heads);

	MPPrintStatus();
	GListPrintStatus();

}

/*
 * BowRemoveReference
 */
void BowRemoveReference(BowResources cres) {
	cres->refcount--;
	if (cres->refcount == 0)
		BowCleanup(cres);
}

static void DeleteAction(), ReturnAction();

static XtActionsRec actionsList[] =
{
  { "ReturnAction", (XtActionProc)ReturnAction },
  { "DeleteAction",  (XtActionProc)DeleteAction  },
};

/*
 * DeleteAction
 */
static void
DeleteAction() {
	BowCleanup(globalcres);
}

/*
 * ReturnAction
 */
static void ReturnAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
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

	return;
}

BowResources ResourcesCreate(int *argcp, char **argv) {
	BowResources cres;
	MemPool mp, tmp;
	char *f, *filename;
	char *logfile;
	char *dbfiles;
	char db[1024], *p;
	struct stat s;

	mp = MPCreate();
	cres = (BowResources)MPCGet(mp, sizeof(struct BowResourcesP));
	cres->mp = mp;
	cres->sources = GListCreateX(mp);
	cres->sourcehooks = GListCreateX(mp);
	cres->renderhooks = GListCreateX(mp);
	cres->mimes = GListCreateX(mp);
	cres->timeouts = GListCreateX(mp);
	cres->oldtimeouts = GListCreateX(mp);
	cres->heads = GListCreateX(mp);
	cres->stacks = GListCreateX(mp);

	cres->cs = SchedulerCreate();

	ResourceAddString(cres, "cache.Directory: /tmp");
	ResourceAddString(cres, "http.userAgent: Bow/2.0alpha");
	ResourceAddString(cres, "mailto.newhead: true");

	if ((dbfiles = getenv("BOW_DBFILES")) == NULL) {
		dbfiles = "~/.bow/resources";
		if (!(p = getenv("HOME")))
			p = "/tmp";

		sprintf(db, "%s/.bow", p);
		mkdir(db, 0700);
		strcat(db, "/resources");
		if (stat(db, &s) != 0) {
			FILE *dbf = fopen(db, "w");
			if (dbf) {
				fprintf(dbf, "%s", default_resource);
				fflush(dbf);
				fclose(dbf);
			}
		}
	}

	f = dbfiles;
	while ((filename = mystrtok(f, ':', &f)) != NULL) {
		ResourceAddFile(cres, filename);
	}

	cres->cc = CacheCreate(cres);

	if (ResourceGetInt(cres, "bow.maxDownloads", &cres->maxiocnt) == NULL) {
		cres->maxiocnt = 4;
	} else if (cres->maxiocnt <= 1) cres->maxiocnt = 1;

	if (ResourceGetBool(cres, "bow.printLoadMessages",
		&cres->printLoadMessages) == NULL) {
		cres->printLoadMessages = false;
	}

	if (ResourceGetBool(cres, "bow.printTaskInfo",
		&cres->printTaskInfo) == NULL) {
		cres->printTaskInfo = false;
	}

	/*
	 * Initialize the Xt stuff.
	 */
	XtToolkitInitialize();

	cres->appcon = XtCreateApplicationContext();
	XSetErrorHandler(xErrorHandler);
	XtAppSetErrorHandler(cres->appcon, xtErrorHandler);
	XtAppSetWarningHandler(cres->appcon, xtWarningHandler);

	XtAppSetFallbackResources(cres->appcon, fallback_resources);
	cres->dpy = XtOpenDisplay(cres->appcon, NULL,
		NULL, "Bow",
		NULL, 0,
		argcp, argv);
	if (cres->dpy == NULL) {
		fprintf(stderr, "Could not open display.\n");
		exit(1);
	}

	XtAppAddActions(cres->appcon, actionsList, XtNumber(actionsList));

	cres->bc = BookmarkCreateContext(cres);

	InitBowBuiltins(cres);

	tmp = MPCreate();
	if ((logfile = ResourceGetFilename(cres, tmp,
		"bow.urlLogFile")) != NULL) {
		cres->logfp = fopen(logfile, "a");
	}
	MPDestroy(tmp);

	cres->plainhooks = RenderGetHooks(cres, "text/plain");

	return(cres);
}

static void ResourcesDestroy(BowResources cres) {
	BowRenderHooks *rhooks;
	BowSourceHooks *shooks;
	GList list;

	if (cres->cc != NULL) CacheDestroy(cres->cc);

	list = cres->renderhooks;
	for (rhooks = (BowRenderHooks *)GListGetHead(list); rhooks != NULL;
		rhooks = (BowRenderHooks *)GListGetNext(list)) {
		if (rhooks->class_destroy != NULL) {
			CMethod(rhooks->class_destroy)(rhooks->class_context);
		}
	}
	list = cres->sourcehooks;
	for (shooks = (BowSourceHooks *)GListGetHead(list); shooks != NULL;
		shooks = (BowSourceHooks *)GListGetNext(list)) {
		if (shooks->class_destroy != NULL) {
			CMethod(shooks->class_destroy)(shooks->class_closure);
		}
	}
	if (cres->db != NULL) XrmDestroyDatabase(cres->db);

	MPDestroy(cres->mp);

	return;
}
