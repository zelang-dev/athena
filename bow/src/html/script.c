/*
 * script.c
 */

#include <stdio.h>
#include <stdlib.h>

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

static void js_append(bow_t web, const char *text, size_t text_len) {
	unsigned long js_buf_size = 0;
	char *js_buf = qjs_get_buf(web, &js_buf_size);
	size_t current_len = !js_buf ? 0 : strlen(js_buf);

	if (current_len + text_len >= js_buf_size) {
		js_buf_size  += text_len + (1024*1024);
		js_buf = realloc(js_buf, js_buf_size);
	}

	qjs_set_buf(web, strcat(js_buf, text), js_buf_size);
}

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
	MLGetText(p, &text, &len);
	if (text == NULL)
		return;

	unsigned long size = 0;
	char *js_buf = qjs_get_buf(li->cres, &size);
	if (size) {
		trace;
		//qjs_run_script(li->cres, NULL, (const char *)text, false);
	}
	qjs_free_buf(li->cres);
}

void HTMLScriptExec(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *text;
	size_t len;
	MLGetText(p, &text, &len);
	if (text == NULL)
		return;

	js_append(li->cres, text, len);
}