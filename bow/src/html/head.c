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

#include "html.h"

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

void HTMLStyleEnd(HTMLInfo li, HTMLEnv env, MLElement p) {
	HTMLEnv fenv;
	if ((fenv = HTMLGetIDEnv(env, TAG_STYLE)) == NULL)
		return;

	char *csstext = HTMLGetEnvText(li->mp, env);
	if (li->css)
		CSSDestroyContext(li->css);

	li->css = CSSParseBuffer(li->wc, csstext, strlen(csstext), NULL, NULL);
}