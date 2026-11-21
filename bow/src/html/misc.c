/*
 * misc.c
 *
 * libhtml - HTML->X renderer
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


#include <ctype.h>
#include "html.h"

static void RenderAnchor _ArgProto((HTMLInfo, HTMLBox, Region));

/*
 * RenderAnchor
 */
static void RenderAnchor(HTMLInfo li, HTMLBox box, Region r) {
}

void HTMLAnchorBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	char *value;
	HTMLBox box;

	if ((value = MLFindAttribute(p, "name")) != NULL) {
		box = HTMLCreateBox(li, env);
		box->width = 2;
		box->height = 2;
		box->render = RenderAnchor;
		HTMLEnvAddBox(li, env, box);
		HTMLAddAnchor(li, box, value, NULL);
	}
	if (MLFindAttribute(p, "href") != NULL) env->anchor = p;
}

HTMLInsertStatus HTMLAnchorInsert(HTMLInfo li, HTMLEnv env, MLElement p) {
	if (MLFindAttribute(p, "href") == NULL) {
		if (MLFindAttribute(p, "name") != NULL) return(HTMLInsertEmpty);
		else return(HTMLInsertReject);
	}
	return(HTMLInsertOK);
}

HTMLFlowAlign HTMLResolveAlignment(HTMLInfo li, HTMLEnv env, MLElement p, HTMLFlowAlign default_align) {
	char *style_attr;
	char *val;
	HTMLAttribID aid;

	/* 1. Check inline style attribute: e.g. style="text-align: center" */
	if (p != NULL && (style_attr = MLFindAttribute(p, "style")) != NULL) {
		val = CSSGetInlineProperty(style_attr, "text-align");
		if (val != NULL) {
			if (strcasecmp(val, "center") == 0) return(FLOW_CENTER_JUSTIFY);
			if (strcasecmp(val, "right") == 0) return(FLOW_RIGHT_JUSTIFY);
			if (strcasecmp(val, "left") == 0) return(FLOW_LEFT_JUSTIFY);
		}
	}

	/* 2. Check CSS stylesheet rules from li->css */
	if (li != NULL && li->css != NULL && li->selectors != NULL) {
		val = CSSFindProperty(li->css, li->selectors, "text-align");
		if (val != NULL) {
			if (strcasecmp(val, "center") == 0) return(FLOW_CENTER_JUSTIFY);
			if (strcasecmp(val, "right") == 0) return(FLOW_RIGHT_JUSTIFY);
			if (strcasecmp(val, "left") == 0) return(FLOW_LEFT_JUSTIFY);
		}
	}

	/* 3. Check legacy HTML align attribute */
	if (p != NULL) {
		aid = HTMLAttributeToID(p, "align");
		if (aid == ATTRIB_CENTER) return(FLOW_CENTER_JUSTIFY);
		if (aid == ATTRIB_RIGHT) return(FLOW_RIGHT_JUSTIFY);
		if (aid == ATTRIB_LEFT) return(FLOW_LEFT_JUSTIFY);
	}

	/* 4. Check class attribute for "center" or class-based centering */
	if (p != NULL && (val = MLFindAttribute(p, "class")) != NULL) {
		if (strcasecmp(val, "center") == 0 || strstr(val, "center") != NULL) {
			return(FLOW_CENTER_JUSTIFY);
		}
	}

	return(default_align);
}

void HTMLHxBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLTagID tagid;

	tagid = HTMLTagToID(env->tag);

	if (tagid == TAG_H1) HTMLSetFontScale(env->fi, 5);
	else if (tagid == TAG_H2) HTMLSetFontScale(env->fi, 4);
	else if (tagid == TAG_H3) HTMLSetFontScale(env->fi, 3);
	else if (tagid == TAG_H4) HTMLSetFontScale(env->fi, 2);
	else if (tagid == TAG_H5) HTMLSetFontScale(env->fi, 1);
	else if (tagid == TAG_H6) HTMLSetFontScale(env->fi, 0);

	env->ff = HTMLResolveAlignment(li, env, p, env->ff);
}

void HTMLHxEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
}

void HTMLFontBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLTagID tagid = HTMLTagToID(env->tag);

	if (tagid == TAG_EM || tagid == TAG_CITE || tagid == TAG_I) {
		HTMLAddFontSlant(env->fi);
	} else if (tagid == TAG_B || tagid == TAG_STRONG) {
		HTMLAddFontWeight(env->fi);
	} else if (tagid == TAG_TT)
		HTMLSetFontFixed(env->fi);
}

void HTMLParaBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	env->ff = HTMLResolveAlignment(li, env, p, env->ff);
}

void HTMLParaEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
}

HTMLInsertStatus HTMLParaInsert(HTMLInfo li, HTMLEnv env, MLElement p) {
  /*
   * If the current environment is <p> then terminate the environment
   * before starting the new one.
   */
	if (HTMLTagToID(env->tag) == TAG_P)
		HTMLEndEnv(li, TAG_P);
	return(HTMLInsertOK);
}

/*
 * HTMLBreak
 */
void HTMLBreak(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAttribID aid;
	HTMLBox box;

	box = HTMLCreateBox(li, env);

	aid = HTMLAttributeToID(p, "clear");
	if (aid == ATTRIB_LEFT) HTMLSetB(box, BOX_CLEAR_LEFT);
	else if (aid == ATTRIB_RIGHT) HTMLSetB(box, BOX_CLEAR_RIGHT);
	else if (aid == ATTRIB_ALL) HTMLSetB(box, BOX_CLEAR_RIGHT | BOX_CLEAR_LEFT);
	else HTMLSetB(box, BOX_LINEBREAK);

	HTMLEnvAddBox(li, env, box);
}

void HTMLAddressBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddFontSlant(env->fi);
	HTMLAddBlankLine(li, env);
}

void HTMLAddressEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddBlankLine(li, env);
}

void HTMLBQBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddLineBreak(li, env);
}

void HTMLBQEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddLineBreak(li, env);
}

void HTMLCenterBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	env->ff = FLOW_CENTER_JUSTIFY;

	HTMLAddLineBreak(li, env);
}

void HTMLCenterEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddLineBreak(li, env);
}

void HTMLDivBegin(HTMLInfo li, HTMLEnv env, MLElement p) {
	env->ff = HTMLResolveAlignment(li, env, p, env->ff);

	HTMLAddLineBreak(li, env);
}

void HTMLDivEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLAddLineBreak(li, env);
}
