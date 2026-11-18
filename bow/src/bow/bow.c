#include <athena.h>

#include "BowP.h"
#include "../bow_archer.xpm"

extern char *fallback_resources[];
static char base_url[MAX_PATH_LEN] = {0};
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
}

static void Cleanup(BowResources cres) {
	if (cres->bc != NULL)
		BookmarkDestroyContext(cres->bc);

	bow_free(cres);

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

char *bow_tag(bowview_t ctx, char *anchor) {
	char *tag = NULL;
	BowRender wn = NULL;
	if (anchor && ctx && ctx->tstack && (wn = StackToRender(ctx->tstack)))
		tag = RenderQuery(wn, anchor);

	return tag;
}

void bow_set_title(bowview_t ctx, const char *name) {
	const char *title = name ? name : bow_tag(ctx, "title");
	if (title) {
		XStoreName(ctx->cres->dpy, ctx->cres->ats->win, title);
		XFlush(ctx->cres->dpy);
	}
}

void bow_window(bow_t web, const char *url, char *secondary_url) {
	/*
	 * Default base URL; this allows filenames to be used on the command line
	 */
	strcpy(base_url, "file:");
	getcwd(base_url + 5, sizeof(base_url) - 5);
	strcat(base_url, "/");
	HeadCreate(web, (url ? RequestCreate(web, (char *)url, base_url) : NULL),
		(secondary_url ? RequestCreate(web, (char *)secondary_url, base_url) : NULL));
}

bow_t bow_alloc(void) {
	bow_t res = NULL;
	MemPool mp = MPCreate();
	if (!(res = (bow_t)MPCGet(mp, sizeof(struct BowResourcesP))))
		return NULL;

	res->mp = mp;
	res->ats = NULL;
	res->qjs = NULL;
	res->js_data = NULL;
	res->sources = NULL;
	res->sourcehooks = NULL;
	res->renderhooks = NULL;
	res->mimes = NULL;
	res->timeouts = NULL;
	res->oldtimeouts = NULL;
	res->heads = NULL;
	res->stacks = NULL;
	res->cs = NULL;
	return res;
}

FORCEINLINE void bow_free(bow_t web) {
	if (!web)
		return;

	if (web->js_data)
		free(web->js_data);

	web->js_data = NULL;
	ResourcesDestroy(web);
}

bow_t bow_create(ats_t *ui) {
	bow_t res;
	MemPool tmp;
	char *f, *filename;
	char *logfile;
	char *dbfiles;
	char db[1024], *p;
	struct stat s;

	if (!(res = bow_alloc()))
		return NULL;

	res->ats = ui;
	res->sources = GListCreateX(res->mp);
	res->sourcehooks = GListCreateX(res->mp);
	res->renderhooks = GListCreateX(res->mp);
	res->mimes = GListCreateX(res->mp);
	res->timeouts = GListCreateX(res->mp);
	res->oldtimeouts = GListCreateX(res->mp);
	res->heads = GListCreateX(res->mp);
	res->stacks = GListCreateX(res->mp);

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

	ui->use_icon = bow_archer;
	if (ats_window_ex(ui, ui->title, ui->width, ui->height, false, fallback_resources,
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
