#include "common.h"
#include <athena.h>

#include "BowP.h"
#include "../bow_archer.xpm"

extern char *fallback_resources[];
static BowResources globalcres = NULL;
static char *default_resource = "bookmark.filename: ~/.bow/bookmarks.html\n"
"cache.directory: ~/.bow/cache\n"
"cache.persist: true\n"
"bow.homeURL: http://www.google.com\n"
"html.propFontPattern: -adobe-helvetica-*-*-*-*-*-*-*-*-*-*-iso8859-1\n"
"view.capFiles: ~/.bow/mailcap:~/.mailcap\n";

static void sigigh_handler(int sig) {}

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
	if (cres->db != NULL)
		XrmDestroyDatabase(cres->db);

	MPDestroy(cres->mp);

	return;
}

static void Cleanup(BowResources cres) {
	if (cres->bc != NULL)
		BookmarkDestroyContext(cres->bc);

	ResourcesDestroy(cres);

	if (cres->logfp != NULL) fclose(cres->logfp);

	GListDestroy(cres->heads);

	MPPrintStatus();
	GListPrintStatus();

}

/*
 * BowAddReference
 */
FORCEINLINE void BowAddReference(BowResources cres) {
	cres->refcount++;
}

/*
 * BowRemoveReference
 */
FORCEINLINE void BowRemoveReference(BowResources cres) {
	cres->refcount--;
	if (cres->refcount == 0)
		Cleanup(cres);
}

/*
 * DeleteAction
 */
static void DeleteAction(Widget w, XEvent *xe, String *params, Cardinal *num_params) {
	Cleanup(globalcres);
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

static XtActionsRec actionsList[] =
{
  {"ReturnAction", (XtActionProc)ReturnAction},
  {"DeleteAction",  (XtActionProc)DeleteAction},
};

char *bow_tag(bowview_t web, char *anchor) {
	char *tag = NULL;
	BowRender wn = NULL;
	if (anchor && web && web->tstack && (wn = StackToRender(web->tstack)))
		tag = RenderQuery(wn, anchor);

	return tag;
}

void bow_set_title(bowview_t web, const char *name) {
	const char *title = name ? name : bow_tag(web, "title");
	if (title) {
		XStoreName(web->cres->dpy, web->cres->ats->win, title);
		XFlush(web->cres->dpy);
	}
}

bow_t bow_create(ats_t *ui, const char *url) {
	bow_t res;
	MemPool mp, tmp;
	char *f, *filename;
	char *logfile;
	char *dbfiles;
	char db[1024], *p;
	struct stat s;

	mp = MPCreate();
	if (!(res = (bow_t)MPCGet(mp, sizeof(struct BowResourcesP))))
		return NULL;

	res->mp = mp;
	res->ats = ui;
	res->sources = GListCreateX(mp);
	res->sourcehooks = GListCreateX(mp);
	res->renderhooks = GListCreateX(mp);
	res->mimes = GListCreateX(mp);
	res->timeouts = GListCreateX(mp);
	res->oldtimeouts = GListCreateX(mp);
	res->heads = GListCreateX(mp);
	res->stacks = GListCreateX(mp);

	res->cs = SchedulerCreate();

	ResourceAddString(res, "cache.Directory: /tmp");
	ResourceAddString(res, "http.userAgent: Bow/0.6.0-chimera_webview");
	ResourceAddString(res, "mailto.newhead: true");

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
		ResourceAddFile(res, filename);
	}

	res->cc = CacheCreate(res);
	if (ResourceGetInt(res, "bow.maxDownloads", &res->maxiocnt) == NULL)
		res->maxiocnt = 4;
	else if (res->maxiocnt <= 1)
		res->maxiocnt = 1;

	if (!ResourceGetBool(res, "bow.printLoadMessages", &res->printLoadMessages))
		res->printLoadMessages = false;

	if (!ResourceGetBool(res, "bow.printTaskInfo", &res->printTaskInfo))
		res->printTaskInfo = false;

	if (ats_window_ex(ui, "Bow", ui->width, ui->height, false, fallback_resources,
		actionsList, XtNumber(actionsList), true, true)) {
		if (!globalcres)
			globalcres = res;

		res->appcon = ui->app_con;
		res->dpy = ui->dpy;
		res->ats = ui;
		res->bc = BookmarkCreateContext(res);
		InitBowBuiltins(res);

		tmp = MPCreate();
		if ((logfile = ResourceGetFilename(res, tmp, "bow.urlLogFile")) != NULL) {
			res->logfp = fopen(logfile, "a");
		}

		MPDestroy(tmp);
		res->plainhooks = RenderGetHooks(res, "text/plain");
		return(res);
	}

	return NULL;
}
