/*
 * head.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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

#include <ctype.h>
#include <string.h>

#include "html.h"

struct HTMLInlineP {
	HTMLInfo li;

	BowGUI wd;
	BowSink wp;
	BowRender wn;
	HTMLInlineInfo ii;

	bool setupcalled;

	HTMLBox box;

	unsigned int width, height, border;
	unsigned int vspace, hspace;
	char *mapurl;
	bool ismap;
	HTMLEnv env;
	bool delayed;

	BowRenderHooks orh;
	void *rc;
};

void HTMLTitleEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	li->title = HTMLGetEnvText(li->mp, env);
}

void HTMLBase(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *url;

	if ((url = MLFindAttribute(p, "href")) != NULL)
		li->burl = url;
}

void HTMLMeta(HTMLInfo li, HTMLEnv env, MLElement p) {
}

/*
 * linkEnd
 */
static void linkEnd(void *closure) {
	HTMLInline isp = (HTMLInline)closure;
	byte *text;
	MIMEHeader mh;
	size_t len;
	SinkGetData(isp->wp, &text, &len, &mh);
	if (len) {
		isp->li->css = CSSParseBuffer(isp->li->wc, text, len, NULL, NULL);
	}
}

/*
 * linkAdd
 */
static void linkAdd(void *closure) {
	(HTMLInline)closure;
}

/*
 * linkInit
 */
static int linkInit(BowSink wp, void *closure) {
	return 0;
}

/*
 * linkMessage
 */
static void linkMessage(void *closure, char *message) {
	HTMLInline isp = (HTMLInline)closure;

	if (isp->li->wn != NULL)
		RenderSendMessage(isp->li->wn, message);
}

static HTMLInline HTMLLinkInline(HTMLInfo li, HTMLEnv env, char *url,
	HTMLInlineInfo *ii, BowRenderHooks *orh, void *rrc) {
	BowSinkHooks hooks;
	HTMLInline isp;
	BowRequest *wr;

	if ((wr = RequestCreate(li->cres, url, li->burl)) == NULL) {
		return(NULL);
	}

	if (!RequestMatchContent2(wr, "text/css")) {
		RequestDestroy(wr);
		return(NULL);
	}

	isp = (HTMLInline)MPCGet(li->mp, sizeof(struct HTMLInlineP));
	memcpy(&(isp->ii), ii, sizeof(HTMLInlineInfo));
	isp->li = li;
	isp->env = env;
	isp->rc = isp;

	memset(&hooks, 0, sizeof(hooks));
	hooks.init = linkInit;
	hooks.add = linkAdd;
	hooks.end = linkEnd;
	hooks.message = linkMessage;

	if (!isp->wp)
		isp->wp = SinkCreate(li->cres, wr);

	SinkSetHooks(isp->wp, &hooks, isp);
	GListAddHead(li->sinks, isp->wp);
	return(isp);
}

void HTMLLink(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *url, *style;
	HTMLInlineInfo ii;
	HTMLEnv fenv;

	if ((fenv = HTMLGetIDEnv(env, TAG_LINK)) == NULL)
		return;

	if ((style = MLFindAttribute(p, "rel")) == NULL
		|| strcasecmp(style, "stylesheet"))
		return;

	if ((url = MLFindAttribute(p, "href")) == NULL)
		return;

	memset(&ii, 0, sizeof(HTMLInlineInfo));
	ii.p = p;

	HTMLLinkInline(li, env, url, &ii, NULL, NULL);
}

void HTMLStyleEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLEnv fenv;
	if ((fenv = HTMLGetIDEnv(env, TAG_STYLE)) == NULL)
		return;

	char *csstext = HTMLGetEnvText(li->mp, env);
	if (li->css)
		CSSDestroyContext(li->css);

	li->css = CSSParseBuffer(li->wc, csstext, strlen(csstext), NULL, NULL);
}