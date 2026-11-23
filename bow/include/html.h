/*
 * html.h
 *
 * libhtml5 - HTML->X renderer
 *
 */
#ifndef __HTML_H__
#define __HTML_H__ 1

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "Bow.h"
#include "BowRender.h"
#include "BowGUI.h"
#include "ml.h"
#include "css.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HTML3.2 tags updated to HTML5 and some internal tags */
typedef enum {
  	/* internal tag */
	TAG_DOCUMENT = 0,

	TAG_A, TAG_ABBR, TAG_ADDRESS, TAG_APPLET, TAG_AREA, TAG_ARTICLE, TAG_ASIDE,
	TAG_AUDIO, TAG_B, TAG_BASE, TAG_BASEFONT, TAG_BDI, TAG_BDO, TAG_BIG,
	TAG_BLOCKQUOTE, TAG_BODY, TAG_BR, TAG_BUTTON, TAG_CANVAS, TAG_CAPTION,
	TAG_CENTER, TAG_CITE, TAG_CODE, TAG_COL, TAG_COLGROUP, TAG_DATA,
	TAG_DATALIST, TAG_DD, TAG_DEL, TAG_DETAILS, TAG_DFN, TAG_DIALOG, TAG_DIR,
	TAG_DIV, TAG_DL, TAG_DOCTYPE, TAG_DT, TAG_EM, TAG_EMBED, TAG_FIELDSET, TAG_FIGCAPTION,
	TAG_FIGURE, TAG_FONT, TAG_FOOTER, TAG_FORM,

	/* weird stuff? */
	TAG_FRAME, TAG_FRAMESET,

	TAG_H1, TAG_H2, TAG_H3, TAG_H4,
	TAG_H5, TAG_H6, TAG_HEAD, TAG_HEADER, TAG_HGROUP, TAG_HR, TAG_HTML, TAG_I,
	TAG_IFRAME, TAG_IMG, TAG_INPUT, TAG_INS, TAG_ISINDEX, TAG_KBD, TAG_LABEL,
	TAG_LEGEND, TAG_LI, TAG_LINK, TAG_LISTING, TAG_MAIN, TAG_MAP, TAG_MARK,
	TAG_MATH, TAG_MENU, TAG_META, TAG_METER, TAG_NAV, TAG_NOSCRIPT,

	/* weird stuff? */
	TAG_NOFRAMES,

	TAG_OBJECT,
	TAG_OL, TAG_OPTGROUP, TAG_OPTION, TAG_OUTPUT, TAG_P, TAG_PARAM, TAG_PICTURE,
	TAG_PLAINTEXT, TAG_PRE, TAG_PROGRESS, TAG_Q, TAG_RP, TAG_RT, TAG_RUBY,
	TAG_S, TAG_SAMP, TAG_SCRIPT, TAG_SEARCH, TAG_SECTION, TAG_SELECT, TAG_SLOT,
	TAG_SMALL, TAG_SOURCE, TAG_SPAN, TAG_STRIKE, TAG_STRONG, TAG_STYLE, TAG_SUB,
	TAG_SUMMARY, TAG_SUP, TAG_SVG, TAG_TABLE, TAG_TBODY, TAG_TD, TAG_TEMPLATE,
	TAG_TEXTAREA, TAG_TFOOT, TAG_TH, TAG_THEAD, TAG_TIME, TAG_TITLE, TAG_TR,
	TAG_TRACK, TAG_TT, TAG_U, TAG_UL, TAG_VAR, TAG_VIDEO, TAG_WBR, TAG_XMP,
	TAG_COUNT
} HTMLTagID;

typedef struct HTMLBoxP       *HTMLBox;
typedef struct HTMLInfoP      *HTMLInfo;
typedef struct HTMLClassP     *HTMLClass;
typedef struct HTMLObjectP    *HTMLObject;
typedef struct HTMLEnvP       *HTMLEnv;
typedef struct HTMLFontP      *HTMLFont;
typedef struct HTMLFontListP  *HTMLFontList;
typedef struct HTMLAnchorP    *HTMLAnchor;
typedef struct HTMLTagP       *HTMLTag;
typedef struct HTMLMapP       *HTMLMap;
typedef struct HTMLAreaP      *HTMLArea;
typedef struct HTMLStateP     *HTMLState;

/*
 * Box callbacks
 */
typedef void (*HTMLSetupProc)(HTMLInfo, HTMLBox);
typedef void (*HTMLDestroyProc)(HTMLInfo, HTMLBox);
typedef void (*HTMLRenderProc)(HTMLInfo, HTMLBox, Region);
typedef void (*HTMLLayoutProc)(HTMLInfo, HTMLBox, HTMLBox);
typedef unsigned int (*HTMLWidthProc)(HTMLInfo, HTMLBox);

/*
 * Tag callbacks
 */
typedef bool (*HTMLTagClampProc)(HTMLInfo, HTMLEnv);
typedef void (*HTMLTagRenderProc)(HTMLInfo, HTMLEnv, MLElement);
typedef bool (*HTMLTagAcceptProc)(HTMLInfo, HTMLObject);

typedef enum
{
  HTMLInsertEmpty,
  HTMLInsertReject,
  HTMLInsertOK
} HTMLInsertStatus;

typedef HTMLInsertStatus (*HTMLTagInsertProc)(HTMLInfo,
							 HTMLEnv, MLElement);
typedef void (*HTMLTagDataProc)(HTMLInfo, HTMLEnv, MLElement);
typedef void (*HTMLTagAddBoxProc)(HTMLInfo, HTMLEnv, HTMLBox);
typedef unsigned int (*HTMLTagWidthProc)(HTMLInfo, HTMLEnv);

typedef enum
{
  FLOW_LEFT_JUSTIFY,
  FLOW_CENTER_JUSTIFY,
  FLOW_RIGHT_JUSTIFY
} HTMLFlowAlign;

/*
 * Environment
 */
typedef enum
{
  HTML_ENV,
  HTML_ELEMENT,
  HTML_TAG,
  HTML_BEGINTAG,
  HTML_ENDTAG
} HTMLObjectType;

struct HTMLObjectP
{
  HTMLObjectType type;
  union
  {
    MLElement p;
    HTMLEnv env;
  } o;
};

struct HTMLEnvP
{
  /* Keep track of html structure */
  HTMLEnv           penv;                  /* parent environment */
  HTMLTag           tag;                   /* tag that defines environment */
  GList             olist;                 /* object list */
  GList             blist;                 /* boxed object list */
  GList             slist;                 /* static object list */
  int               pass;

  /* Keep track of HTML rendering */
  bool              visited;               /* used by boxify functions */
  MLElement         anchor;                /* anchor to use in environment */
  HTMLFont          fi;                    /* font to use in environment */
  HTMLFlowAlign     ff;                    /* alignment in flow box */
  void              *closure;              /* environment specific data */
};

typedef enum
{
  BOX_NONE                = 0x00000000,    /* no flags set */
  BOX_SPACE               = 0x00000001,    /* horizontal space */
  BOX_LINEBREAK           = 0x00000002,    /* line break, duh. */
  BOX_FLOAT_LEFT          = 0x00000008,    /* float to the left edge */
  BOX_FLOAT_RIGHT         = 0x00000010,    /* float to the right edge */
  BOX_VCENTER             = 0x00000020,    /* vertically center in parent */
  BOX_PUSHED_LEFT         = 0x00000040,    /* box pushed left by floater */
  BOX_PUSHED_RIGHT        = 0x00000080,    /* box pushed right by floater */
  BOX_SELECT              = 0x00000100,    /* anchor box selected */
  BOX_TOPLEVEL            = 0x00000200,    /* toplevel box ? */
  BOX_CLEAR_LEFT          = 0x00000400,    /* clear past left aligned */
  BOX_CLEAR_RIGHT         = 0x00000800     /* clear past right aligned */
} HTMLBoxFlags;

#define HTMLTestB(a, b)     (((a)->bflags & (b)) != 0)
#define HTMLClearB(a, b)    ((a)->bflags &= ~(b))
#define HTMLSetB(a, b)      ((a)->bflags |= (b))

/* This is still too big */
struct HTMLBoxP
{
  HTMLEnv      env;                 /* */
  int          baseline;            /* level of alignment */
  int          x, y;                /* upper-left coordinates */
  unsigned int width, height;       /* external dimensions */
  HTMLBoxFlags bflags;              /* misc. worthless flags */

  /* rendering functions */
  HTMLRenderProc render;            /* called when output needed */
  HTMLDestroyProc destroy;          /* called when destroyed */
  HTMLSetupProc  setup;             /* called by parent box */
  HTMLLayoutProc layout;            /* child box layout */
  HTMLWidthProc maxwidth;           /* called to return maximum width */
  void         *closure;            /* state for functions */
};

struct HTMLAnchorP
{
  HTMLBox box;
  char *name;
  MLElement p;
};

struct HTMLStateP
{
  int x, y;
};

struct HTMLClassP
{
  MemPool     mp;
  bool        font_setup_done;
  Display     *dpy;
  XFontStruct *defaultFont;
  HTMLFontList prop;
  HTMLFontList fixed;
  GList       oldstates;
  GList       fonts;
  GList       contexts;
  CSSContext  css;
};

struct HTMLInfoP
{
  /* X */
  Widget      widget;
  Display     *dpy;
  Window      win;
  GC          gc;
  Pixel       anchor_color;
  Pixel       anchor_select_color;
  Pixel       fg, bg;

  /* www lib */
  BowContext  wc;
  BowResources cres;
  BowGUI      wd;
  BowSink     wp;
  BowTask     wt;
  BowRender   wn;
  GList       sinks;
  GList       loads;

  /* HTML */
  CSSContext  css;
  bool        reload;
  unsigned int maxwidth, maxheight;
  MemPool     mp;
  MLState     hs;
  HTMLClass   lc;
  HTMLFont    cfi;
  HTMLBox     firstbox;
  HTMLState   ps;

  HTMLEnv     searchenv;
  HTMLObject  searchobj;

  HTMLEnv     topenv;               /* document environment */
  GList       envstack;             /* tag hierarchy */
  GList       selectors;            /* CSS selector list */
  GList       oldselectors;         /* old CSS selector list */
  GList       endstack;             /* pending end tags */

  GList       alist;                /* anchor box list */
  BowTimeOut  sto;              	/* select timeout */
  HTMLAnchor  sa;                   /* selected anchor */

  int         delayed;
  char        *url;                 /* original URL */
  char        *burl;                /* base URL */
  char        *title;               /* title text */
  HTMLAnchor  over;
  GList       maps;
  bool        cancel;
  bool	      formatted;	    /* inside PRE tags - djhjr */
  bool        noframeset;
  bool        framesonly;
  GList       framesets;            /* list of frames */

  /* configurable goodies */
  int         textLineSpace;
  bool        printHTMLErrors;
  int         tmargin, bmargin;     /* top, bottom margins */
  int         rmargin, lmargin;     /* right, left margins */
  unsigned int inlineTimeOut;
  unsigned int selectTimeOut;       /* select timeout time */
  unsigned int tableCellInfinity;   /* infinite width for table cell */
  bool        flowDebug;
  bool        constraintDebug;
  bool        printTags;            /* print tag and data hierarchy */
  bool 		  is_repainting;
};

typedef enum
{
  ATTRIB_UNKNOWN,
  ATTRIB_LEFT,
  ATTRIB_RIGHT,
  ATTRIB_TOP,
  ATTRIB_BOTTOM,
  ATTRIB_CENTER,
  ATTRIB_MIDDLE,
  ATTRIB_ALL
} HTMLAttribID;

/*
 * inline.c
 */
typedef struct
{
  MLElement p;
  void      *closure;
} HTMLInlineInfo;

typedef struct HTMLInlineP *HTMLInline;

HTMLInline HTMLCreateInline(HTMLInfo, HTMLEnv, char *,
				       HTMLInlineInfo *,
				       BowRenderHooks *, void *);
HTMLBox HTMLInlineToBox(HTMLInline);
void HTMLInlineDestroy(HTMLInline);

/*
 * text.c
 */
HTMLBox HTMLCreateTextBox(HTMLInfo, HTMLEnv, char *, size_t);
char *HTMLGetEnvText(MemPool, HTMLEnv);
void HTMLStringSpacify(char *, size_t);
void HTMLAddLineBreak(HTMLInfo, HTMLEnv);
void HTMLAddBlankLine(HTMLInfo, HTMLEnv);
HTMLFlowAlign HTMLResolveAlignment(HTMLInfo, HTMLEnv, MLElement, HTMLFlowAlign);

/*
 * layout.c
 */
HTMLBox HTMLCreateBox(HTMLInfo, HTMLEnv);
void HTMLLayoutBox(HTMLInfo, HTMLBox, HTMLBox);
void HTMLSetupBox(HTMLInfo, HTMLBox);
void HTMLRenderBox(HTMLInfo, Region, HTMLBox);
void HTMLDestroyBox(HTMLInfo, HTMLBox);
unsigned HTMLGetBoxWidth(HTMLInfo, HTMLBox);

/*
 * html.c
 */
HTMLAttribID HTMLAttributeToID(MLElement, char *);
HTMLTag HTMLGetTag(MLElement);
HTMLTag HTMLTagIDToTag(HTMLTagID);
bool HTMLHandleTag(HTMLInfo, HTMLTag, MLElement);
HTMLEnv HTMLFindEnv(HTMLInfo, HTMLTagID);
void HTMLDelayLayout(HTMLInfo);
void HTMLContinueLayout(HTMLInfo);
HTMLTagID HTMLTagToID(HTMLTag);
void HTMLHandler(void *, MLElement);
HTMLEnv HTMLPopEnv(HTMLInfo, HTMLTagID);
void HTMLStartEnv(HTMLInfo, HTMLTagID, MLElement);
void HTMLEndEnv(HTMLInfo, HTMLTagID);
unsigned int HTMLGetMaxWidth(HTMLInfo, HTMLEnv);
void HTMLEnvAddBox(HTMLInfo, HTMLEnv, HTMLBox);
void HTMLStart(HTMLInfo);
void HTMLFinish(HTMLInfo);
HTMLEnv HTMLGetIDEnv(HTMLEnv, HTMLTagID);

/*
 * font.c
 */
XFontStruct *HTMLGetFont(HTMLInfo, HTMLEnv);
void HTMLSetupFonts(HTMLInfo);
void HTMLFreeFonts(HTMLClass);
void HTMLAddFontSlant(HTMLFont);
void HTMLAddFontWeight(HTMLFont);
void HTMLAddFontScale(HTMLFont);
void HTMLSetFontScale(HTMLFont, int);
HTMLFont HTMLDupFont(HTMLInfo, HTMLFont);
void HTMLSetFontFixed(HTMLFont);
void HTMLSetFontProp(HTMLFont);

/*
 * load.c
 */
int HTMLLoadAnchor(HTMLInfo, HTMLAnchor, int, int, char *, bool);
void HTMLPrintAnchor(HTMLInfo, HTMLAnchor, int, int, bool);
char *HTMLMakeURL(HTMLInfo, MemPool, char *);
int HTMLLoadURL(HTMLInfo, char *, char *, char *);
void HTMLPrintURL(HTMLInfo, char *);
void HTMLFindName(HTMLInfo, char *);
void HTMLAddAnchor(HTMLInfo, HTMLBox, char *, MLElement);

/*
 * module.c
 */
void HTMLSetFinalPosition(HTMLInfo);
void HTMLRepaintFromDOM(HTMLInfo);

/*
 * flow.c
 */
HTMLBox HTMLCreateFlowBox(HTMLInfo, HTMLEnv, unsigned int);
void HTMLFinishFlowBox(HTMLInfo, HTMLBox);

/*
 * map.c
 */
char *HTMLFindMapURL(HTMLInfo, char *, int, int);

/*
 *
 * Tag/Env handlers
 *
 */

/*
 * form.c
 */
void HTMLFormBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLFormEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLInput(HTMLInfo, HTMLEnv, MLElement);
void HTMLOptionEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLSelectBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLSelectEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLTextareaEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLButton(HTMLInfo li, HTMLEnv env, MLElement p);
void HTMLButtonEnd(HTMLInfo li, HTMLEnv env, MLElement p);

/*
 * inline.c
 */
void HTMLImg(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLImgInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLCanvas(HTMLInfo, HTMLEnv, MLElement);
void HTMLCanvasResize(int w, int h);
void HTMLCanvasClearRect(int x, int y, int w, int h);
void HTMLCanvasStrokeSegments(XSegment *segs, int nsegs, const char *color_str);
void HTMLCanvasFlush(void);
void HTMLCanvasGetBounds(int *x, int *y, int *w, int *h);

/*
 * script.c
 */
void HTMLScript(HTMLInfo, HTMLEnv, MLElement);
void HTMLScriptEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLScriptExec(HTMLInfo, HTMLEnv, MLElement);

/*
 * hr.c
 */
void HTMLHorizontalRule(HTMLInfo, HTMLEnv, MLElement);

/*
 * list.c
 */
HTMLInsertStatus HTMLLIInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLLIBegin(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLDTInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLDTBegin(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLDDInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLDDBegin(HTMLInfo, HTMLEnv, MLElement);
bool HTMLListAccept(HTMLInfo, HTMLObject);
bool HTMLDLAccept(HTMLInfo, HTMLObject);
void HTMLULBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLListEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLOLBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLDLBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLListAddBox(HTMLInfo, HTMLEnv, HTMLBox);
void HTMLItemAddBox(HTMLInfo, HTMLEnv, HTMLBox);
unsigned int HTMLItemWidth(HTMLInfo, HTMLEnv);
void HTMLItemEnd(HTMLInfo, HTMLEnv, MLElement);

/*
 * text.c
 */
void HTMLPreBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLPreEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLPlainBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLPlainEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLFillData(HTMLInfo, HTMLEnv, MLElement);
void HTMLPreData(HTMLInfo, HTMLEnv, MLElement);
void HTMLPlainData(HTMLInfo, HTMLEnv, MLElement);

/*
 * misc.c
 */
void HTMLHxBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLHxEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLParaBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLParaEnd(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLParaInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLBreak(HTMLInfo, HTMLEnv, MLElement);
void HTMLAddressBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLAddressEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLAnchorBegin(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLAnchorInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLBQBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLBQEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLFontBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLDivBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLDivEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLCenterBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLCenterEnd(HTMLInfo, HTMLEnv, MLElement);

/*
 * table.c
 */
bool HTMLTableAccept(HTMLInfo, HTMLObject);
void HTMLTableBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLTableEnd(HTMLInfo, HTMLEnv, MLElement);
bool HTMLTRAccept(HTMLInfo, HTMLObject);
HTMLInsertStatus HTMLTRInsert(HTMLInfo, HTMLEnv, MLElement);
bool HTMLTRClamp(HTMLInfo, HTMLEnv);
void HTMLTRBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLTREnd(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLTDInsert(HTMLInfo, HTMLEnv, MLElement);
bool HTMLTDClamp(HTMLInfo, HTMLEnv);
void HTMLTDBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLTDEnd(HTMLInfo, HTMLEnv, MLElement);
unsigned int HTMLTDWidth(HTMLInfo, HTMLEnv);
void HTMLTableAddBox(HTMLInfo, HTMLEnv, HTMLBox);
void HTMLTDAddBox(HTMLInfo, HTMLEnv, HTMLBox);

/*
 * head.c
 */
void HTMLBase(HTMLInfo, HTMLEnv, MLElement);
void HTMLTitleEnd(HTMLInfo, HTMLEnv, MLElement);
void HTMLMeta(HTMLInfo, HTMLEnv, MLElement);
void HTMLStyleEnd(HTMLInfo li, HTMLEnv env, MLElement p);
void HTMLLink(HTMLInfo li, HTMLEnv env, MLElement p);

/*
 * map.c
 */
bool HTMLMapAccept(HTMLInfo, HTMLObject);
HTMLInsertStatus HTMLAreaInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLAreaBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLMapBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLMapEnd(HTMLInfo, HTMLEnv, MLElement);

/*
 * frame.c
 */
void HTMLIFrame(HTMLInfo, HTMLEnv, MLElement);
void HTMLFrame(HTMLInfo, HTMLEnv, MLElement);
HTMLInsertStatus HTMLFrameInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLFrameSetBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLFrameSetEnd(HTMLInfo, HTMLEnv, MLElement);
bool HTMLFrameSetAccept(HTMLInfo, HTMLObject);
HTMLInsertStatus HTMLFrameSetInsert(HTMLInfo, HTMLEnv, MLElement);
void HTMLNoFramesBegin(HTMLInfo, HTMLEnv, MLElement);
void HTMLNoFramesEnd(HTMLInfo, HTMLEnv, MLElement);

/*
A lightweight zero-dependency C89 HTML5 parser, HTML5 tag lookup table, HTML entity decoding,
CSS selector querying, DOM Structures and DOM tree mutation helpers, for `html5.c`.

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
typedef struct HtmlNode_S HtmlDom_t;

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

typedef struct {
	HtmlDom_t **items;
	size_t count;
	size_t capacity;
} HtmlNodeList;

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

/* Parsing & Lifecycle */
HtmlDom_t *html_parse(const char *html);
void dom_free(HtmlDom_t *node);
HtmlDom_t *dom_parent(HtmlDom_t *node);

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

#endif
