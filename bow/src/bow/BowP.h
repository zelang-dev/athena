/*
 * BowP.h
 */
#ifndef __BOW_P_H__
#define __BOW_P_H__

#include "Bow.h"
#include "BowSource.h"
#include "BowGUI.h"
#include "BowRender.h"
#include "BowAuth.h"
#include "BowStack.h"

typedef struct BowCacheP *BowCache;
typedef struct BookmarkContextP *BookmarkContext;
typedef struct BowSchedulerP *BowScheduler;

struct BowContextP {
	MemPool mp;

	Widget toplevel;
	Widget file;
	Widget www;
	Widget open;
	Widget back;
	Widget reload;
	Widget cancel;
	Widget quit;
	Widget home;
	Widget addmark;
	Widget viewmark;
	Widget message;
	Widget dup;
	Widget source;
	Widget save;
	Widget openpop;
	Widget savepop;
	Widget findpop;
	Widget bookpop;
	Widget url;
	Widget help;
	Widget bookmark;
	Widget find;

	BowStack       tstack;  /* top stack */
	BowStack       cstack;  /* current stack */
	MemPool			tmp;

	BowResources cres;
	ats_t *athena;			/* `athena` handle */
};

/*
 * Bow runtime context
 */
struct BowResourcesP {
	int              maxiocnt;          /* maximum IOs */
	bool             printLoadMessages;
	bool             printTaskInfo;
	MemPool          mp;                /* memory pool */
	XrmDatabase      db;                /* message database */

	BowCache     cc;

	/* Lists of stuff we need to know about */
	GList            sourcehooks;       /* list of registered sources */
	GList            renderhooks;       /* list of registered renderers */
	GList            mimes;             /* list of mimes */
	GList            renderers;         /* list of active renderers */
	GList            sources;           /* list of active sources */
	GList            timeouts;          /* list of timeouts */
	GList            oldtimeouts;       /* list of old timeouts */
	GList            heads;             /* list of heads */
	GList            stacks;            /* list of stacks */

	/* */
	void			(*authui_callback)();
	void 			*authui_closure;

	XtAppContext     appcon;            /* X application context */
	size_t           id;                /* next unique ID */

	Display *dpy;

	BowScheduler 	cs;					/* task scheduler context */
	BowContext   	bmcontext;			/* head associated with bookmarks */
	BowRenderHooks 	*plainhooks;		/* plain text renderer */
	BookmarkContext  bc;				/* bookmark routines context */
	FILE *logfp;			/* log file */
	int	refcount;          	/* reference count */
	ats_t *ats;				/* `athena` handle */
	Bow_QJS *qjs;			/* `Bow_QJS` integration global handle */
	void *js_data;			/* `data` from `qjs_fetchfile/qjs_readfile` */
	void *active_html;		/* active HTMLInfo for live JS DOM repaints */
};

/*
 *
 * Prototypes
 *
 */

/*
 * cache.c
 */
BowCache CacheCreate(BowResources);
void CacheDestroy(BowCache);
bool CacheIsDiskCached(BowCache, char *);
int CacheWrite(BowCache, BowSource);
BowSourceHooks *CacheGetHooks(BowCache);

/*
 * data.c
 */
void SourceGetData(BowSource, byte **, size_t *, MIMEHeader *);

/*
 * gui.c
 */
BowGUI GUICreateToplevel(BowContext, Widget,
	GUISizeCallback, void *);
void GUIReset(BowGUI);
void GUIAddRender(BowGUI, BowRender);

/*
 * bookmark.c
 */
BookmarkContext BookmarkCreateContext(BowResources);
void BookmarkDestroyContext(BookmarkContext);
void BookmarkAdd(BookmarkContext, char *, char *);
void BookmarkShow(BookmarkContext);

/*
 * head.c
 */
void HeadCreate(BowResources, BowRequest *,
	BowRequest *);
void HeadDestroy(BowContext);
void HeadPrintMessage(BowContext, char *);
void HeadPrintURL(BowContext, char *);

Widget CreateDialog(Widget, char *,	void (*)(), void (*)(), void (*)(), XtPointer);
Widget GetDialogWidget(Widget);

/*
 * main.c
 */
void BowAddReference(BowResources);
void BowRemoveReference(BowResources);

/*
 * callback.c
 */
bool MessageCallback(void *, char *);
bool ActionCallback(void *, BowRequest *, char *);
bool RedrawCallback(void *);
bool ResizeCallback(void *);

/*
 * builtin.c
 */
void InitBowBuiltins(BowResources);

/*
 * view.c
 */
void ViewOpen(BowResources, BowRequest *);

/*
 * download.c
 */
void DownloadOpen(BowResources, BowRequest *);

/*
 * stack.c
 */
BowStack StackCreateToplevel(BowContext, Widget);
BowGUI StackToGUI(BowStack);
BowRender StackToRender(BowStack);
void StackSetRender(BowStack, BowRenderHooks *);

/*
 * resource.c
 */
char *ResourceGetStringP(BowResources, char *);

/*
 * cmisc.c
 */
BowSourceHooks *SourceGetHooks(BowResources, char *);

/*
 * task.c
 */
BowScheduler SchedulerCreate(void);

#endif
