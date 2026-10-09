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

#include <athena.h>
#include "MyDialog.h"
#include "bow/BowP.h"
#include "bow_archer.xpm"

static void sigigh_handler _ArgProto(());
static void xtWarningHandler _ArgProto((String));
static int xErrorHandler _ArgProto((Display *, XErrorEvent *));
static void BowCleanup _ArgProto((BowResources));
static BowResources ResourcesCreate _ArgProto((ats_t *, int *, char **));
static void ResourcesDestroy _ArgProto((BowResources));

extern char *fallback_resources[];

static BowResources globalcres = NULL;
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

	ats_t ui = {0};
	ui.use_icon = bow_archer;
	globalcres = ResourcesCreate(&ui, &argc, argv);
	if (globalcres) {
		/*
		* Default base URL; this allows filenames to be used on the
		* command line
		*/
		strcpy(base_url, "file:");
		getcwd(base_url + 5, sizeof(base_url) - 5);
		strcat(base_url, "/");

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

	return -1;
}

/*
 * sigiqh_handler
 */
static void sigigh_handler(int sig) {
}

/*
 * xErrorHandler
 */
static int xErrorHandler(Display *dpy, XErrorEvent *xe) {
	char buf[256] = {0};
	XGetErrorText(dpy, xe->error_code, buf, sizeof(buf));
	fprintf(stderr, "X error: %d\n", buf);
	return 0;
}

/*
 * xtErrorHandler
 */
 static FORCEINLINE void xtErrorHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
 }

/*
 * xtWarningHandler
 */
 static FORCEINLINE void xtWarningHandler(String msg) {
	fprintf(stderr, "%s\n", msg);
}

/*
 * BowAddReference
 */
FORCEINLINE void BowAddReference(BowResources cres) {
	cres->refcount++;
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
FORCEINLINE void BowRemoveReference(BowResources cres) {
	cres->refcount--;
	if (cres->refcount == 0)
		BowCleanup(cres);
}

static void DeleteAction(Widget w, XEvent *xe, String *params, Cardinal *num_params);
static void ReturnAction(Widget w, XEvent *xe, String *params, Cardinal *num_params);

static XtActionsRec actionsList[] =
{
  { "ReturnAction", (XtActionProc)ReturnAction },
  { "DeleteAction",  (XtActionProc)DeleteAction  },
};

/*
 * DeleteAction
 */
static void DeleteAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
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
}

BowResources ResourcesCreate(ats_t *ats, int *argcp, char **argv) {
	BowResources cres;
	MemPool mp, tmp;
	char *f, *filename;
	char *logfile;
	char *dbfiles;
	char db[1024], *p;
	struct stat s;

	mp = MPCreate();
	if (!(cres = (BowResources)MPCGet(mp, sizeof(struct BowResourcesP))))
		return NULL;

	cres->mp = mp;
	cres->ats = ats;
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
	ResourceAddString(cres, "http.userAgent: Bow/0.6.0-chimera_webview");
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
	if (ResourceGetInt(cres, "bow.maxDownloads", &cres->maxiocnt) == NULL)
		cres->maxiocnt = 4;
	else if (cres->maxiocnt <= 1)
		cres->maxiocnt = 1;

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
	if (ats_window_ex(ats, "Bow", 768, 600, false, fallback_resources, actionsList, XtNumber(actionsList), true)) {
		cres->appcon = ats->app_con;
		cres->dpy = ats->dpy;
		cres->bc = BookmarkCreateContext(cres);
		InitBowBuiltins(cres);

		tmp = MPCreate();
		if ((logfile = ResourceGetFilename(cres, tmp, "bow.urlLogFile")) != NULL) {
			cres->logfp = fopen(logfile, "a");
		}

		MPDestroy(tmp);
		cres->plainhooks = RenderGetHooks(cres, "text/plain");
		return(cres);
	}

	return NULL;
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
