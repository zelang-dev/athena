#ifndef _HTML5_PARSER
#define _HTML5_PARSER

/*

A lightweight upgraded from HTML 3.2 zero-dependency C89 HTML5 parser, HTML5 tag lookup table, HTML entity decoding, CSS selector querying, and DOM tree mutation helpers.

## Features

- **Complete HTML5 + Legacy Tag Table**: `HTMLTagID` and `HTMLTagInfo` sorted alphabetically with void (`is_selfclose`) flags.
- **Fast O(log N) Tag Lookup**: Case-insensitive binary search (`html_lookup_tag`) directly over string slices.
- **HTML Entity Decoding & Encoding**: Named entities (`&amp;`, `&copy;`, `&mdash;`, etc.), decimal (`&#233;`), and hex (`&#x2615;`) UTF-8 decoding.

- **Raw Tag Content Extraction**:
  - `dom_get_raw_content(node)` — returns unescaped raw contents of `<script>`, `<style>`, or inner content of any element.
  - `dom_get_inner_html(node)` — serializes the child nodes of any element.
  - `dom_get_text_content(node)` — recursively extracts all plain text inside a subtree.
  - `dom_get_elements_by_tag_id(root, tag_id)` / `dom_get_elements_by_tag_name(root, name)` — collects matching elements into a `HtmlNodeList`.
  - `dom_extract_tag_contents(root, tag_id, &count)` — one-call helper returning a `char **` array of raw contents for every matching tag (such as all `<script>` blocks).

- **DOM Tree & Attribute Mutation Helpers**:
  - `dom_create_element`, `dom_create_text`
  - `dom_append_child`, `dom_insert_before`, `dom_remove_child`, `dom_replace_child`
  - `dom_set_text_content`
  - `dom_get_attr`, `dom_has_attr`, `dom_set_attr`, `dom_remove_attr`

- **DOM Serialization**: Pretty-printed and compact HTML5 output via `dom_serialize`.
- **CSS Selector Engine**: (`dom_query_selector` / `dom_query_selector_all`):** Supports tag names, `*`, `#id`, `.class`, `[attr]`, `[attr=val]`, descendant combinator (` `), direct child combinator (`>`), and comma-separated selector lists (`,`).

*/

#include "common.h"
#include <html.h>
#include "html5.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    HTMLTagID id;
    const char *name;
    bool is_selfclose;
} HTMLTagInfo;

/* Fast O(log N) case-insensitive tag lookup */
const HTMLTagInfo *html_lookup_tag(const char *tag_name, size_t len);
const HTMLTagInfo *html_lookup_tag_by_id(HTMLTagID id);

/* HTML Entity Decoding & Encoding */
char *html_decode_entities(const char *src, size_t len);
char *html_encode_entities(const char *src, bool is_attr);

/* Lightweight DOM Structures & Mutation Helpers */
typedef enum {
    NODE_DOCUMENT,
    NODE_ELEMENT,
    NODE_TEXT,
    NODE_COMMENT
} HtmlNodeType;

typedef struct HtmlAttr {
    char *name;
    char *value;
    struct HtmlAttr *next;
} HtmlAttr;

typedef struct Html_ShadowNode HtmlDom_t;
struct Html_ShadowNode {
    HtmlNodeType type;
    HTMLTagID tag_id;
    char *tag_name;
    char *text;
    HtmlAttr *attrs;
    HtmlDom_t *parent;
    HtmlDom_t *first_child;
    HtmlDom_t *last_child;
    HtmlDom_t *next_sibling;
};

typedef struct {
    HtmlDom_t **items;
    size_t count;
    size_t capacity;
} HtmlNodeList;

/* Parsing & Lifecycle */
HtmlDom_t *html_parse(const char *html);
void dom_free(HtmlDom_t *node);

/* Debug */
void dom_print(const HtmlDom_t *node, int depth);

/* Node Creation & Tree Mutation */
HtmlDom_t *dom_create_element(const char *tag_name);
HtmlDom_t *dom_create_text(const char *text);
HtmlDom_t *dom_create_comment(const char *comment);
void dom_append_child(HtmlDom_t *parent, HtmlDom_t *child);
void dom_insert_before(HtmlDom_t *parent, HtmlDom_t *new_node, HtmlDom_t *ref_node);

/* Detaches 'child' from 'parent' without freeing it; returns 'child' or NULL */
HtmlDom_t *dom_remove_child(HtmlDom_t *parent, HtmlDom_t *child);

/* Replaces 'old_child' with 'new_child' under 'parent'; returns detached 'old_child' or NULL */
HtmlDom_t *dom_replace_child(HtmlDom_t *parent, HtmlDom_t *new_child, HtmlDom_t *old_child);

/* Attribute Helpers */
const char *dom_get_attr(const HtmlDom_t *elem, const char *name);
bool dom_has_attr(const HtmlDom_t *node, const char *name);
void dom_set_attr(HtmlDom_t *elem, const char *name, const char *value);
bool dom_remove_attr(HtmlDom_t *elem, const char *name);

/* Raw Tag Content & Subtree Extraction Helpers */

/* Returns unescaped raw inner text of an element (ideal for <script> / <style>) */
char *dom_get_raw_content(const HtmlDom_t *node);

/* Returns concatenated plain text across all descendant text nodes */
char *dom_get_text_content(const HtmlDom_t *node);

void dom_set_text_content(HtmlDom_t *node, const char *text);

/* Serializes only the children of 'node' back to HTML */
char *dom_get_inner_html(const HtmlDom_t *node, bool pretty);

/* Finds all descendant elements matching tag_id or tag_name */
HtmlNodeList *dom_get_elements_by_tag_id(const HtmlDom_t *root, HTMLTagID tag_id);
HtmlNodeList *dom_get_elements_by_tag_name(const HtmlDom_t *root, const char *tag_name);
void dom_node_list_free(HtmlNodeList *list);

/* Extracts an array of raw content strings for every matching tag (e.g. TAG_SCRIPT).
 * Caller must free each string and the returned array (or use html_string_array_free). */
char **dom_extract_tag_contents(const HtmlDom_t *root, HTMLTagID tag_id, size_t *out_count);
char **dom_extract_tag_contents_by_name(const HtmlDom_t *root, const char *tag_name, size_t *out_count);
void html_string_array_free(char **arr, size_t count);

/* CSS Selector Querying */
HtmlDom_t *dom_query_selector(HtmlDom_t *root, const char *selector);
HtmlNodeList *dom_query_selector_all(HtmlDom_t *root, const char *selector);
void dom_node_list_free(HtmlNodeList *list);

/* DOM Serialization */

char *dom_serialize(const HtmlDom_t *node, bool pretty);

#ifdef __cplusplus
}
#endif

#endif /* _HTML5_PARSER */
