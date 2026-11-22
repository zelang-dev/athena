/*
 * bow_engine.h
 *
 * Public API for the Bow central document-routing engine.
 *
 * Include this header in any translation unit that needs to route HTML
 * content through the dual-parser pipeline (html5.c + javascript.c for
 * modern documents, ml.c for legacy HTML 3.2).
 *
 * Typical usage from src/html/module.c:
 *
 *   #include "bow_engine.h"
 *
 *   // In HTMLAdd callback (streaming, partial chunk):
 *   bow_engine_add(li->hs, (char *)data, len);
 *
 *   // In HTMLEnd callback (final chunk, triggers full pipeline):
 *   bow_engine_end(li->hs, li->cres, (char *)data, len);
 */

#ifndef __BOW_ENGINE_H__
#define __BOW_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>   /* size_t */
#include "ml.h"       /* MLState */
typedef void (*jsMessage_cb)(const char *handler_name, const char *payload, void *closure);

/*
 * bow_t is BowResources (struct BowResourcesP *) — the central per-instance
 * context that carries the QJS engine pointer (web->qjs).
 *
 * If Bow.h or BowP.h is already included before this header, BowResources is
 * already defined.  We provide a compatible typedef under the bow_t alias only
 * when it has not already been declared.
 */
#ifndef __BOW_H__
/* Forward declaration only — avoids pulling in X11/QuickJS headers */
typedef struct BowResourcesP *bow_t;
#else
/* Bow.h is already included; alias BowResources → bow_t */
typedef BowResources bow_t;
#endif

/* ============================================================================
 * Streaming pipeline entry points (called from HTMLAdd / HTMLEnd)
 * ============================================================================ */

/*
 * bow_engine_add
 *
 * Pass a partial chunk of incoming HTML bytes through the pipeline.
 * For partial (non-final) chunks this is a transparent forwarding call to
 * MLAddData — the HTML5 pipeline is deferred until bow_engine_end().
 */
void bow_engine_add(MLState hs, char *data, size_t len);

/*
 * bow_engine_end
 *
 * Process the final (complete) HTML document chunk.
 *
 *  • If the document is detected as HTML5 / scripted:
 *      html_parse() → qjs_run_script() → dom_serialize() → MLEndData()
 *  • Otherwise (legacy HTML 3.2 / plain text):
 *      MLEndData() directly (zero overhead, existing behaviour preserved).
 *
 * Parameters:
 *   hs   – active MLState for the current render context
 *   web  – bow_t (BowResources) carrying the QJS context; pass NULL to skip JS
 *   data – complete accumulated document bytes
 *   len  – byte length of data
 */
void bow_engine_end(MLState hs, bow_t web, char *data, size_t len);

/* ============================================================================
 * One-shot entry point (complete document already in memory)
 * ============================================================================ */

/*
 * bow_engine_process
 *
 * Routes a complete HTML document buffer through the appropriate pipeline and
 * calls both MLAddData and MLEndData internally.  Use for cache hits, source
 * view, and other cases where the full buffer is available immediately.
 */
void bow_engine_process(MLState hs, bow_t web, char *data, size_t len);

/* ============================================================================
 * Dynamic content streaming
 * ============================================================================ */

/*
 * bow_engine_inject_html
 *
 * Inject a dynamically generated HTML fragment into the active layout flow.
 * The fragment is parsed by html5.c, any embedded <script> tags are executed,
 * and the resulting serialised subtree is fed to MLAddData.
 *
 * Parameters:
 *   hs   – active MLState
 *   web  – bow_t for JS context (may be NULL)
 *   html – null-terminated HTML fragment string (not modified)
 */
void bow_engine_inject_html(MLState hs, bow_t web, const char *html);

/* ============================================================================
 * DOM snapshot utility
 * ============================================================================ */

/*
 * bow_engine_get_dom_snapshot
 *
 * Serialises the current live DOM (web->qjs->dom_root) to a malloc'd HTML
 * string.  Returns NULL if no DOM is live or serialisation fails.
 * Caller must free() the returned string.
 */
char *bow_engine_get_dom_snapshot(bow_t web);

/* ============================================================================
 * Streaming pipeline entry points (called from HTMLAdd / HTMLEnd)
 * ============================================================================ */

/*
 * bow_engine_add
 *
 * Pass a partial chunk of incoming HTML bytes through the pipeline.
 * For partial (non-final) chunks this is a transparent forwarding call to
 * MLAddData — the HTML5 pipeline is deferred until bow_engine_end().
 */
void bow_engine_add(MLState hs, char *data, size_t len);

/*
 * bow_engine_end
 *
 * Process the final (complete) HTML document chunk.
 *
 *  • If the document is detected as HTML5 / scripted:
 *      html_parse() → qjs_run_script() → dom_serialize() → MLEndData()
 *  • Otherwise (legacy HTML 3.2 / plain text):
 *      MLEndData() directly (zero overhead, existing behaviour preserved).
 *
 * Parameters:
 *   hs   – active MLState for the current render context
 *   web  – bow_t (BowResources) carrying the QJS context; pass NULL to skip JS
 *   data – complete accumulated document bytes
 *   len  – byte length of data
 */
void bow_engine_end(MLState hs, bow_t web, char *data, size_t len);

/* ============================================================================
 * One-shot entry point (complete document already in memory)
 * ============================================================================ */

/*
 * bow_engine_process
 *
 * Routes a complete HTML document buffer through the appropriate pipeline and
 * calls both MLAddData and MLEndData internally.  Use for cache hits, source
 * view, and other cases where the full buffer is available immediately.
 */
void bow_engine_process(MLState hs, bow_t web, char *data, size_t len);

/* ============================================================================
 * Dynamic content streaming
 * ============================================================================ */

/*
 * bow_engine_inject_html
 *
 * Inject a dynamically generated HTML fragment into the active layout flow.
 * The fragment is parsed by html5.c, any embedded <script> tags are executed,
 * and the resulting serialised subtree is fed to MLAddData.
 *
 * Parameters:
 *   hs   – active MLState
 *   web  – bow_t for JS context (may be NULL)
 *   html – null-terminated HTML fragment string (not modified)
 */
void bow_engine_inject_html(MLState hs, bow_t web, const char *html);

/* ============================================================================
 * DOM snapshot utility
 * ============================================================================ */

/*
 * bow_engine_get_dom_snapshot
 *
 * Serialises the current live DOM (web->qjs->dom_root) to a malloc'd HTML
 * string.  Returns NULL if no DOM is live or serialisation fails.
 * Caller must free() the returned string.
 */
char *bow_engine_get_dom_snapshot(bow_t web);

/*
 * WebView API: set the local directory used to resolve relative
 * subresources (e.g., <img src="logo.png">) inside data: documents.
 * Pass NULL to fall back to the process current working directory.
 */
void bow_assetDatadir_set(const char *);

/* For JS usage: window.bow.postMessage("action", { key: "value" }) */
void bow_jsMessage_set(jsMessage_cb cb, void *closure);

/* Evaluate JS from C host (for native-to-JS replies) */
void bow_reEval_js(bow_t web, const char *script);

#ifdef __cplusplus
}
#endif

#endif /* __BOW_ENGINE_H__ */
