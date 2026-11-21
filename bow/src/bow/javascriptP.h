#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#ifndef _JAVASCRIPT_P_H__
#define _JAVASCRIPT_P_H__

#include "BowP.h"

struct JS_EventsList {
	char *type;
	char *id;
	JSValue callback;
	JS_Listener *next;
};

struct JS_Mutation {
	JSValue callback;
	HtmlDom_t *target;
	JS_Observer *next;
};

/* Async Engine Module Promise Resolution Container */
struct JS_ESMPromise {
	char *module_name;
	JSValue resolve;
	JSValue reject;
	JS_Promise *next;
};

typedef enum {
	CMD_RECT, CMD_LINE, CMD_TEXT
} CanvasActionType;

struct JS_CanvasAction {
	CanvasActionType type;
	int x, y, w, h;
	unsigned long color;
	char *text;
	XFontStruct *font;
	JS_Canvas *next;
};

typedef enum {
	TASK_FETCH, TASK_MODULE_IMPORT, TASK_SETTIMEOUT
} TaskType;

struct JS_AsyncTask {
	TaskType type;
	useconds_t delay;
	char *url;
	JSValue resolve;
	JSValue reject;
	JSValue timeout;
	JS_Task *next;
};

struct HtmlNode_S {
	HtmlNodeType type;
	HTMLTagID tag_id;

	int x, y, width, height;
	char *tag_name;
	char *text;

	HtmlAttr *attrs;
	HtmlDom_t *parent;
	HtmlDom_t *first_child;
	HtmlDom_t *last_child;
	HtmlDom_t *next_sibling;

	JSValue js_object;
	JSValue js_onclick;
	JS_Listener *listeners;
};

struct QJS_Info {
	int scroll_offset_y, js_dom_count;
	size_t js_buf_size, js_script_size;
	JSClassID js_class_id, observer_class_id, nodelist_class_id;
	JSValue js_this;
	JSValue js_obs;
	char *js_buf;
	char *js_script;
	Pixmap canvas_buffer;
	int canvas_w, canvas_h;
	Widget render_canvas;
	GC gc;
	XFontStruct *font_normal;
	XFontStruct *font_bold;
	XFontStruct *font_italic;
	HtmlDom_t *dom_root;
	JSContext *ctx;
	JSRuntime *rt;
	JS_Observer *observers;
	JS_Promise *module_promises;
	JS_Canvas *canvas_queue;
	JS_Task *task_queue;
	pthread_mutex_t canvas_mutex;
	pthread_mutex_t task_mutex;
};

/* Additional DOM helper functions implemented in html5.c */
HtmlDom_t *dom_previous_sibling(const HtmlDom_t *node);
void dom_set_inner_html(HtmlDom_t *node, const char *html);
void dom_set_outer_html(HtmlDom_t *node, const char *html);
HtmlDom_t *dom_clone_node(const HtmlDom_t *node, bool deep);

#endif /* _JAVASCRIPT_P_H__ */
