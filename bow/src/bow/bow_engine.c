/*
 * bow_engine.c
 *
 * Central document-routing engine for the Bow browser.
 *
 * This module forms the bridge between:
 *   - src/bow/html5.c   — modern HTML5 DOM parser + CSS selector engine
 *   - src/bow/javascript.c — QuickJS JS runtime with DOM bindings
 *   - src/common/ml.c   — legacy HTML 3.2 state-machine parser (layout backend)
 *   - src/html/         — the existing box/flow layout pipeline
 *
 * Architecture — dual-parser router:
 *
 *   Incoming document bytes
 *         │
 *         ▼
 *   bow_engine_detect_mode()
 *         │
 *   ┌─────┴──────────────────────┐
 *   │  HTML5 mode                │  Legacy / fringe mode
 *   │  (DOCTYPE html, or any     │  (HTML 3.2, partial docs,
 *   │   document with <script>)  │   plain text snippets …)
 *   │                            │
 *   ▼                            ▼
 *  html_parse()           MLAddData / MLEndData
 *   │                            │
 *   ▼                            └──► HTMLHandler → layout
 *  qjs_init / qjs_parse
 *   │
 *   ├─► execute_script_blocks()   (JS mutates DOM)
 *   │
 *   ▼
 *  bow_engine_serialize_dom()     (DOM → HTML text)
 *   │
 *   ▼
 *  MLAddData / MLEndData          (inject into layout pipeline)
 *   │
 *   └──► HTMLHandler → layout
 *
 * Error handling:
 *   - JS exceptions are caught by run_javascript() in javascript.c and
 *     printed to stderr; execution continues with the DOM as mutated up
 *     to that point.
 *   - If html5 parsing or QJS initialisation fails the engine falls back
 *     to the legacy ml.c path so the document is never silently dropped.
 *   - If DOM serialisation produces an empty buffer the raw source bytes
 *     are handed to the legacy parser directly (safe fallback).
 *
 * Usage (called from src/html/module.c):
 *
 *   BowEngineState *es = bow_engine_init(li, data, len);
 *   bow_engine_run(es);       // parse + JS + re-inject
 *   bow_engine_destroy(es);
 *
 *   Or, for the streaming add/end pattern:
 *
 *   bow_engine_add(li, data, len);   // called from HTMLAdd
 *   bow_engine_end(li, data, len);   // called from HTMLEnd – runs full pipeline
 */

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "html.h"            /* HTMLInfo, MLState, MLAddData, MLEndData … */
#include "javascriptP.h"     /* HtmlDom_t, qjs_*, bow_t */

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/*
 * bow_engine_has_html5_doctype
 *
 * Returns true if the document bytes begin with <!DOCTYPE html …>  (case-
 * insensitive) or contain at least one HTML5-only tag that did not exist in
 * HTML 3.2 (article, section, nav, header, footer, main, aside, canvas …).
 * This is intentionally liberal so that any modern page gets the full pipeline.
 */
static bool bow_engine_has_html5_doctype(const char *data, size_t len)
{
	size_t i;
	/* Quick prefix scan – most pages start with <!DOCTYPE */
	if (len >= 9) {
		const char *p = data;
		/* Skip leading whitespace / BOM */
		while ((size_t)(p - data) < len && isspace((unsigned char)*p))
			p++;

		if ((size_t)(p - data) + 9 <= len) {
			/* strncasecmp replacement (portable) */
			static const char doctype[] = "<!DOCTYPE";
			bool match = true;
			size_t k;
			for (k = 0; k < 9 && match; k++)
				match = (tolower((unsigned char)p[k]) == tolower((unsigned char)doctype[k]));
			if (match)
				return true;
		}
	}

	/*
	 * Secondary heuristic: look for HTML5 semantic tags or <script> blocks.
	 * We scan at most the first 4 kB to stay cheap.
	 */
	static const char *html5_tags[] = {
		"<article", "<section", "<nav", "<header", "<footer",
		"<main", "<aside", "<canvas", "<video", "<audio",
		"<figure", "<figcaption", "<details", "<summary",
		"<dialog", "<meter", "<progress", "<time", "<mark",
		"<data", "<output", "<template", "<slot", "<hgroup",
		"<search", NULL
	};

	size_t scan_len = len < 4096 ? len : 4096;
	for (i = 0; html5_tags[i]; i++) {
		size_t tlen = strlen(html5_tags[i]);
		if (tlen > scan_len)
			continue;
		size_t j;
		for (j = 0; j + tlen <= scan_len; j++) {
			bool eq = true;
			size_t k;
			for (k = 0; k < tlen && eq; k++)
				eq = (tolower((unsigned char)data[j + k]) ==
				      (unsigned char)html5_tags[i][k]);
			if (eq)
				return true;
		}
	}

	return false;
}

/*
 * bow_engine_has_script
 *
 * Returns true if the document contains any <script element, indicating we
 * must run the JS pipeline regardless of DOCTYPE.
 */
static bool bow_engine_has_script(const char *data, size_t len)
{
	static const char needle[] = "<script";
	size_t nlen = sizeof(needle) - 1;
	size_t i;
	if (len < nlen)
		return false;
	for (i = 0; i + nlen <= len; i++) {
		bool eq = true;
		size_t k;
		for (k = 0; k < nlen && eq; k++)
			eq = (tolower((unsigned char)data[i + k]) == (unsigned char)needle[k]);
		if (eq)
			return true;
	}
	return false;
}

/* ============================================================================
 * Parser-mode detection
 * ============================================================================ */

typedef enum {
	ENGINE_MODE_LEGACY,   /* ml.c only – HTML 3.2 / plain text */
	ENGINE_MODE_HTML5,    /* html5.c → JS → serialise → ml.c */
} BowEngineMode;

static BowEngineMode bow_engine_detect_mode(const char *data, size_t len)
{
	if (!data || len == 0)
		return ENGINE_MODE_LEGACY;

	if (bow_engine_has_html5_doctype(data, len) ||
	    bow_engine_has_script(data, len))
		return ENGINE_MODE_HTML5;

	return ENGINE_MODE_LEGACY;
}

/* ============================================================================
 * DOM serialisation helpers
 * ============================================================================ */

/*
 * bow_engine_dom_to_html
 *
 * Serialises the entire post-JS DOM back to an HTML string that the legacy
 * ml.c layout pipeline can process.  Returns a malloc'd string; caller frees.
 * Returns NULL on failure.
 */
static char *bow_engine_dom_to_html(HtmlDom_t *dom)
{
	if (!dom)
		return NULL;

	char *html = dom_serialize(dom, false /* compact */);
	return html;  /* may be NULL if dom_serialize fails */
}

/* ============================================================================
 * Error-safe script execution
 * ============================================================================ */

/*
 * bow_engine_run_scripts
 *
 * Runs all <script> blocks found in 'dom' through QuickJS.  Any JS exceptions
 * are already handled inside javascript.c (run_javascript prints to stderr and
 * continues), so this function is purely a convenience wrapper that ensures
 * the QJS context is initialised before we ask it to execute scripts.
 *
 * Returns true if script execution completed (even if individual scripts
 * threw exceptions), false if the QJS engine could not be initialised at all.
 */
static bool bow_engine_run_scripts(bow_t web, HtmlDom_t *dom)
{
	if (!web || !dom)
		return false;

	/* Ensure QJS runtime is live */
	if (!qjs_is_running(web)) {
		if (!qjs_init(web)) {
			fprintf(stderr, "bow_engine: failed to initialise QuickJS\n");
			return false;
		}
	}

	/* Bind parsed DOM as active document root before running <script> tags so that JS
	 * `document.*` calls operate on the correct tree.*/
	qjs_bind_dom(web, dom);
	qjs_run_script(web, dom, NULL, false);
	return true;
}

/* ============================================================================
 * HTML5 pipeline path
 * ============================================================================ */

/*
 * bow_engine_html5_pipeline
 *
 * Full modern pipeline:
 *   1. Parse raw bytes with html5.c → HtmlDom_t tree
 *   2. Run all embedded <script> blocks through QuickJS
 *   3. Serialise the (possibly JS-mutated) DOM back to HTML
 *   4. Feed the serialized HTML into the ml.c / layout pipeline
 *
 * On any failure the function falls back gracefully to feeding the original
 * raw bytes through ml.c so the page is never lost.
 *
 * Parameters:
 *   hs    – MLState already initialised by the caller (from HTMLInfo.hs)
 *   web   – bow_t (BowResources) for QJS context access
 *   data  – raw HTML bytes
 *   len   – length of data
 *   is_end – true when this is the final chunk (calls MLEndData)
 */
static void bow_engine_html5_pipeline(MLState hs, bow_t web,
                           char *data, size_t len, bool is_end)
{
	HtmlDom_t *dom = NULL;
	char      *serialized = NULL;
	char      *null_terminated = NULL;

	if (!data || len == 0)
		goto fallback;

	/* 1. Null-terminate for the parser */
	null_terminated = (char *)malloc(len + 1);
	if (!null_terminated)
		goto fallback;
	memcpy(null_terminated, data, len);
	null_terminated[len] = '\0';

	/* 2. HTML5 parse */
	dom = html_parse(null_terminated);
	if (!dom) {
		fprintf(stderr, "bow_engine: html_parse failed, falling back to legacy ml.c\n");
		goto fallback;
	}

	/* 3. Run JavaScript (non-fatal) */
	if (web) {
		if (!bow_engine_run_scripts(web, dom)) {
			/*
			 * QJS failed to init – script execution is skipped but we
			 * continue with the DOM as parsed (no JS mutations).
			 */
			fprintf(stderr, "bow_engine: JS execution skipped (QJS unavailable)\n");
		}
	}

	/* 4. Serialise the (possibly mutated) DOM */
	serialized = bow_engine_dom_to_html(dom);
	if (!serialized || serialized[0] == '\0') {
		/* Serialisation produced nothing – fall back to raw bytes */
		fprintf(stderr, "bow_engine: DOM serialisation empty, falling back to legacy ml.c\n");
		free(serialized);
		goto fallback;
	}

	/* 5. Feed serialized HTML into ml.c layout pipeline */
	{
		size_t slen = strlen(serialized);
		if (is_end)
			MLEndData(hs, serialized, slen);
		else
			MLAddData(hs, serialized, slen);
	}

	/* 6. Layout boxes (including <canvas>) now exist; fire window.onload / DOMContentLoaded */
	if (is_end && web && qjs_is_running(web)) {
		qjs_fire_onload(web);
	}

	/* Cleanup: keep dom alive if bound to web->qjs->dom_root for live JS */
	free(serialized);
	if (!web || !web->qjs || web->qjs->dom_root != dom)
		dom_free(dom);

	free(null_terminated);
	return;

fallback:
	/*
	 * Any failure above: hand the original bytes directly to the legacy
	 * ml.c parser.  This guarantees we never drop a document silently.
	 */
	if (dom)           dom_free(dom);
	if (null_terminated) free(null_terminated);

	if (is_end)
		MLEndData(hs, data, len);
	else
		MLAddData(hs, data, len);
}

/* ============================================================================
 * Public API — called from src/html/module.c (HTMLAdd / HTMLEnd)
 * ============================================================================ */

/*
 * bow_engine_add
 *
 * Drop-in replacement for MLAddData in the streaming add callback.
 * Routes through the HTML5 pipeline if the content warrants it, otherwise
 * passes through directly to ml.c.
 *
 * NOTE: For the add (non-final) path the HTML5 pipeline is intentionally NOT
 * invoked on partial chunks because html_parse() requires a complete document.
 * Partial chunks are always forwarded to MLAddData.  The full pipeline runs
 * only on the final chunk in bow_engine_end().
 */
void bow_engine_add(MLState hs, char *data, size_t len)
{
	if (!hs || !data || len == 0)
		return;

	/*
	 * Partial chunk: always pass through to ml.c so incremental rendering
	 * of non-scripted legacy pages is unaffected.
	 */
	MLAddData(hs, data, len);
}

/*
 * bow_engine_end
 *
 * Drop-in replacement for MLEndData in the streaming end callback.
 * On the final chunk we have the complete document bytes; now we can decide
 * which parser path to take.
 *
 * Parameters:
 *   hs   – MLState for the current render context
 *   web  – bow_t for JS engine access (may be NULL → JS skipped)
 *   data – complete HTML bytes accumulated so far
 *   len  – length
 */
void bow_engine_end(MLState hs, bow_t web, char *data, size_t len)
{
	if (!hs || !data || len == 0) {
		if (hs)
			MLEndData(hs, data, len);
		return;
	}

	BowEngineMode mode = bow_engine_detect_mode(data, len);

	if (mode == ENGINE_MODE_HTML5) {
		bow_engine_html5_pipeline(hs, web, data, len, true /* is_end */);
	} else {
		/* Legacy path – direct pass-through, zero overhead */
		MLEndData(hs, data, len);
	}
}

/*
 * bow_engine_process
 *
 * One-shot entry point for callers that have the full document buffer already
 * (e.g. cache hits, source view, internal pages).
 *
 * Parameters:
 *   hs   – MLState for the current render context
 *   web  – bow_t for JS engine access (may be NULL)
 *   data – complete HTML bytes
 *   len  – length
 */
void bow_engine_process(MLState hs, bow_t web, char *data, size_t len)
{
	if (!hs || !data || len == 0)
		return;

	BowEngineMode mode = bow_engine_detect_mode(data, len);

	if (mode == ENGINE_MODE_HTML5) {
		bow_engine_html5_pipeline(hs, web, data, len, true /* is_end */);
	} else {
		MLAddData(hs, data, len);
		MLEndData(hs, data, len);
	}
}

/* ============================================================================
 * Dynamic content streaming – inject HTML generated at runtime back into layout
 * ============================================================================ */

/*
 * bow_engine_inject_html
 *
 * Streams a dynamically generated HTML fragment (e.g. produced by a JS
 * document.write() shim or by server-sent content) into the currently active
 * ml.c layout pipeline.
 *
 * The fragment is first passed through html5.c so that any nested <script>
 * tags inside it are also executed.  The resulting serialized subtree is then
 * fed to MLAddData so it flows into the existing layout context.
 *
 * Parameters:
 *   hs      – active MLState
 *   web     – bow_t for JS context (may be NULL)
 *   html    – null-terminated HTML fragment string
 */
void bow_engine_inject_html(MLState hs, bow_t web, const char *html)
{
	HtmlDom_t *dom;
	char      *serialized;
	size_t     slen;

	if (!hs || !html || html[0] == '\0')
		return;

	/* Parse the fragment */
	dom = html_parse(html);
	if (!dom) {
		/* Last resort: treat as opaque text */
		slen = strlen(html);
		MLAddData(hs, (char *)html, slen);
		return;
	}

	/* Run any embedded scripts */
	if (web && qjs_is_running(web))
		qjs_run_script(web, dom, NULL, false);

	/* Serialise and inject */
	serialized = bow_engine_dom_to_html(dom);
	if (serialized && serialized[0] != '\0') {
		slen = strlen(serialized);
		MLAddData(hs, serialized, slen);
		free(serialized);
	} else {
		/* Serialization failed – inject raw fragment */
		free(serialized);
		slen = strlen(html);
		MLAddData(hs, (char *)html, slen);
	}

	dom_free(dom);
}

/* ============================================================================
 * Convenience – get a serialized snapshot of the current live DOM
 * ============================================================================ */

/*
 * bow_engine_get_dom_snapshot
 *
 * Returns a malloc'd string with the current DOM serialized to HTML.
 * Returns NULL if no DOM is available or serialization fails.
 * Caller is responsible for freeing the returned string.
 */
char *bow_engine_get_dom_snapshot(bow_t web)
{
	if (!web || !web->qjs || !web->qjs->dom_root)
		return NULL;

	return bow_engine_dom_to_html(web->qjs->dom_root);
}
