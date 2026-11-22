/*
 * script.c
 */

#include <stdio.h>

#include "html.h"

struct HTMLInlineP {
	HTMLInfo li;

	BowGUI wd;
	BowSink wp;
	BowRender wn;
	HTMLInlineInfo ii;

	bool setupcalled;

	HTMLBox box;

	char *mapurl;
	bool ismap;
	HTMLEnv env;
	bool delayed;

	BowRenderHooks orh;
	void *rc;
};

/*
 * jsEnd
 */
static void jsEnd(void *closure) {
	HTMLInline isp = (HTMLInline)closure;
	byte *text;
	MIMEHeader mh;
	size_t len;
	SinkGetData(isp->wp, &text, &len, &mh);
	if (len)
		qjs_run_script(isp->li->cres, NULL, (const char *)text, false);
}

/*
 * jsAdd
 */
static void jsAdd(void *closure) {
	(HTMLInline)closure;
}

/*
 * jsInit
 */
static int jsInit(BowSink wp, void *closure) {
	return 0;
}

/*
 * jsMessage
 */
static void jsMessage(void *closure, char *message) {
	HTMLInline isp = (HTMLInline)closure;

	if (isp->li->wn != NULL)
		RenderSendMessage(isp->li->wn, message);
}

/*
 *
 * Public functions
 *
 */
HTMLInline HTMLScriptInline(HTMLInfo li, HTMLEnv env, char *url,
	HTMLInlineInfo *ii, BowRenderHooks *orh, void *rrc) {
	BowSinkHooks hooks;
	HTMLInline isp;
	BowRequest *wr;

	if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL) {
		return(NULL);
	}

	if (!RequestMatchContent2(wr, "text/javascript")) {
		RequestDestroy(wr);
		return(NULL);
	}

	isp = (HTMLInline)MPCGet(li->mp, sizeof(struct HTMLInlineP));
	memcpy(&(isp->ii), ii, sizeof(HTMLInlineInfo));
	isp->li = li;
	isp->env = env;
	isp->rc = isp;

	memset(&hooks, 0, sizeof(hooks));
	hooks.init = jsInit;
	hooks.add = jsAdd;
	hooks.end = jsEnd;
	hooks.message = jsMessage;

	if (!isp->wp)
		isp->wp = SinkCreate(li->cres, wr);

	SinkSetHooks(isp->wp, &hooks, isp);
	GListAddHead(li->sinks, isp->wp);
	return(isp);
}

/*
 * HTMLScript
 */
void HTMLScript(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *url;
	HTMLInlineInfo ii;

	if (li != NULL && li->is_repainting)
		return;

	if (!qjs_is_running(li->cres))
		qjs_init(li->cres);

	if ((url = MLFindAttribute(p, "src")) == NULL)
		return;

	memset(&ii, 0, sizeof(HTMLInlineInfo));
	ii.p = p;

	HTMLScriptInline(li, env, url, &ii, NULL, NULL);
}

void HTMLScriptEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *text;
	size_t len;

	if (li != NULL && li->is_repainting)
		return;

	MLGetText(p, &text, &len);
	if (text == NULL)
		return;

	unsigned long size = 0;
	char *js_buf = qjs_get_script(li->cres, &size);
	if (size) {
		size = strlen(js_buf) - len - 2;
		js_buf[size] = '\0';
		qjs_run_script(li->cres, NULL, (const char *)js_buf, false);
	}
	qjs_free_script(li->cres);
}

void HTMLScriptExec(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *text;
	size_t len;

	if (li != NULL && li->is_repainting)
		return;

	MLGetText(p, &text, &len);
	if (text == NULL)
		return;

	qjs_append_script(li->cres, text, len);
}