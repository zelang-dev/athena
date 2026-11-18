#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include "BowP.h"

/* Enumerate common legacy handlers */
typedef enum LegacyEventType {
	EVENT_CLICK,
	EVENT_LOAD,
	EVENT_CHANGE,
	EVENT_MOUSEOVER,
	EVENT_MOUSEOUT,
	EVENT_KEYDOWN,
	EVENT_KEYUP,
	EVENT_COUNT
} LegacyEventType;

const char *LEGACY_EVENT_NAMES[EVENT_COUNT] = {
	"onclick", "onload", "onchange", "onmouseover", "onmouseout", "onkeydown", "onkeyup"
};

typedef struct {
	char key[64];
	char value[512];
} JS_Attribute;

struct JS_EventsList {
	char *type;
	char *id;
	JSValue callback;
	JS_Listener *next;
};

struct JS_CSS_Styles {
	char *name;
	char *value;
	JS_CSSVariable *next;
};

struct JS_ShadowDOM {
	HtmlNodeType type;
	int x, y, width, height;
	char tagName[64];
	char *textContent;

	JS_Attribute *attributes;
	size_t attr_count;
	size_t attr_capacity;

	JS_Dom *parent;
	JS_Dom **children;
	size_t child_count;
	size_t child_capacity;

	JSValue js_object;
	JSValue js_onclick;
	JS_Listener *listeners;
};

struct JS_Mutation {
	JSValue callback;
	JS_Dom *target;
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

struct Bow_QJS_Info {
	int scroll_offset_y, js_dom_count;
	size_t js_buf_size;
	JSClassID js_class_id, observer_class_id;
	JSValue js_this;
	JSValue js_obs;
	char *js_buf;
	Pixmap canvas_buffer;
	int canvas_w, canvas_h;
	Widget render_canvas;
	GC gc;
	XFontStruct *font_normal;
	XFontStruct *font_bold;
	XFontStruct *font_italic;
	JS_Dom *dom_root;
	JSContext *ctx;
	JSRuntime *rt;
	JS_Observer *observers;
	JS_CSSVariable *style_vars;
	JS_Promise *module_promises;
	JS_Canvas *canvas_queue;
	JS_Task *task_queue;
	pthread_mutex_t canvas_mutex;
	pthread_mutex_t task_mutex;
};

static Bow_QJS g_app = {0};
static volatile bool g_run_pipeline = true;

static JS_Dom *js_create_dom(HtmlNodeType type, const char *name_or_text) {
	JS_Dom *node = calloc(1, sizeof(JS_Dom));
	if (!node)
		return NULL;

	node->type = type;
	if (type == NODE_ELEMENT)
		strncpy(node->tagName, name_or_text, sizeof(node->tagName) - 1);
	else
		node->textContent = strdup(name_or_text);

	node->js_object = JS_UNDEFINED;
	node->js_onclick = JS_UNDEFINED;
	return node;
}

static void js_free_dom(JS_Dom *node) {
	if (!node)
		return;

	size_t i;
	for (i = 0; i < node->child_count; i++) {
		js_free_dom(node->children[i]);
		node->children[i] = NULL;
	}

	if (node->children)
		free(node->children);

	if (node->textContent)
		free(node->textContent);

	if (node->attributes)
		free(node->attributes);

	if (node->listeners) {
		JS_Listener *el = node->listeners;
		while (el) {
			JS_Listener *tmp = el->next;
			free(el->type);
			free(el->id);
			JS_FreeValue(g_app.ctx, el->callback);
			free(el);
			el = tmp;
		}
	}

	JS_FreeValue(g_app.ctx, node->js_onclick);
	JS_FreeValue(g_app.ctx, node->js_object);
	free(node);
	node = NULL;
}

void qjs_exit(void) {
	g_run_pipeline = false;
	usleep(50000); /* Allow async threads to exit gracefully */

	if (g_app.canvas_queue) {
		pthread_mutex_lock(&g_app.canvas_mutex);
		JS_Canvas *cmd = g_app.canvas_queue;
		while (cmd) {
			JS_Canvas *next = cmd->next;
			free(cmd->text);
			free(cmd);
			cmd = next;
		}
		pthread_mutex_unlock(&g_app.canvas_mutex);
	}

	if (g_app.task_queue) {
		pthread_mutex_lock(&g_app.task_mutex);
		JS_Task *task = g_app.task_queue;
		while (task) {
			JS_Task *next = task->next;
			free(task->url);
			JS_FreeValue(g_app.ctx, task->resolve);
			JS_FreeValue(g_app.ctx, task->reject);
			JS_FreeValue(g_app.ctx, task->timeout);
			free(task);
			task = next;
		}
		pthread_mutex_unlock(&g_app.task_mutex);
	}

	if (g_app.style_vars) {
		JS_CSSVariable *sv = g_app.style_vars;
		while (sv) {
			JS_CSSVariable *next = sv->next;
			free(sv->name);
			free(sv->value);
			free(sv);
			sv = next;
		}
	}

	if (g_app.observers) {
	JS_Observer *mo = g_app.observers;
		while (mo) {
			JS_Observer *next = mo->next;
			JS_FreeValue(g_app.ctx, mo->callback);
			free(mo);
			mo = next;
		}
	}

	js_free_dom(g_app.dom_root);
	g_app.dom_root = NULL;
	if (g_app.render_canvas) {
		Display *dpy = XtDisplay(g_app.render_canvas);
		if (g_app.gc)
			XFreeGC(dpy, g_app.gc);

		if (g_app.font_normal)
			XFreeFont(dpy, g_app.font_normal);

		if (g_app.font_bold)
			XFreeFont(dpy, g_app.font_bold);

		if (g_app.font_italic)
			XFreeFont(dpy, g_app.font_italic);
		g_app.render_canvas = NULL;
	}

	if (g_app.js_buf)
		free(g_app.js_buf);

	g_app.js_buf = NULL;
	if (g_app.ctx) {
		JS_FreeValue(g_app.ctx, g_app.js_obs);
		JS_FreeValue(g_app.ctx, g_app.js_this);
		js_std_free_handlers(g_app.rt);
		JS_FreeContext(g_app.ctx);
		JS_FreeRuntime(g_app.rt);
		g_app.ctx = NULL;
		g_app.rt = NULL;
	}
}

static void notify_mutation(const char *type, JS_Dom *target) {
	JS_Observer *curr = g_app.observers;
	while (curr) {
		if (curr->callback && (curr->target == NULL || curr->target == target)) {
			JSValue record = JS_NewObject(g_app.ctx);
			JS_SetPropertyStr(g_app.ctx, record, "type",
				JS_NewString(g_app.ctx, type));
			JSValue ret = JS_Call(g_app.ctx, curr->callback, JS_UNDEFINED, 1, &record);
			JS_FreeValue(g_app.ctx, ret);
			JS_FreeValue(g_app.ctx, record);
			//break;
		}
		curr = curr->next;
	}
}

static void js_append_child(JS_Dom *parent, JS_Dom *child) {
	if (parent->child_count >= parent->child_capacity) {
		parent->child_capacity = parent->child_capacity == 0 ? 8 : parent->child_capacity * 2;
		parent->children = realloc(parent->children, parent->child_capacity * sizeof(JS_Dom *));
	}

	if (parent->children) {
		parent->children[parent->child_count++] = child;
		child->parent = parent;
	}

	JS_Observer *curr = g_app.observers;
	while (curr) {
		JSValue argv = {JS_DupValue(g_app.ctx, child->js_object)};
		JSValue ret = JS_Call(g_app.ctx, curr->callback, JS_UNDEFINED, 1, &argv);
		JS_FreeValue(g_app.ctx, argv);
		JS_FreeValue(g_app.ctx, ret);
		curr = curr->next;
	}
}

static void js_add_attribute(JS_Dom *node, const char *key, const char *value) {
	if (node->attr_count >= node->attr_capacity) {
		node->attr_capacity = node->attr_capacity == 0 ? 4 : node->attr_capacity * 2;
		node->attributes = realloc(node->attributes, node->attr_capacity * sizeof(JS_Attribute));
	}

	if (node->attributes) {
		strncpy(node->attributes[node->attr_count].key, key, sizeof(node->attributes->key) - 1);
		strncpy(node->attributes[node->attr_count].value, value, sizeof(node->attributes->value) - 1);
		node->attr_count++;
		notify_mutation("attributes", node);
	}
}

static bool js_should_autoclose(const char *open, const char *next) {
	if (strcasecmp(open, "p") == 0 && (strcasecmp(next, "p") == 0 || strcasecmp(next, "div") == 0
		|| strcasecmp(next, "table") == 0 || strcasecmp(next, "tr") == 0 || strcasecmp(next, "td") == 0))
		return true;
	if ((strcasecmp(open, "td") == 0 || strcasecmp(open, "th") == 0) && (strcasecmp(next, "td") == 0
		|| strcasecmp(next, "th") == 0 || strcasecmp(next, "tr") == 0 || strcasecmp(next, "table") == 0))
		return true;
	if (strcasecmp(open, "tr") == 0 && (strcasecmp(next, "tr") == 0 || strcasecmp(next, "table") == 0))
		return true;

	return false;
}

static void js_serialize_html(JS_Dom *node, char **buf, size_t *cap, bool outer) {
	if (!node) return;
	int i;
	if (node->type == NODE_TEXT) {
		size_t len = strlen(node->textContent);
		while (strlen(*buf) + len + 1 >= *cap) {
			*cap *= 2;
			*buf = realloc(*buf, *cap);
		}

		strcat(*buf, node->textContent);
		return;
	}

	if (outer) {
		while (strlen(*buf) + strlen(node->tagName) + 3 >= *cap) {
			*cap *= 2; *buf = realloc(*buf, *cap);
		}

		strcat(*buf, "<");
		strcat(*buf, node->tagName);
		for (i = 0; i < node->attr_count; i++) {
			size_t attr_len = strlen(node->attributes[i].key) + strlen(node->attributes[i].value) + 4;
			while (strlen(*buf) + attr_len >= *cap) {
				*cap *= 2;
				*buf = realloc(*buf, *cap);
			}

			strcat(*buf, " ");
			strcat(*buf, node->attributes[i].key);
			strcat(*buf, "=\"");
			strcat(*buf, node->attributes[i].value);
			strcat(*buf, "\"");
		}

		strcat(*buf, ">");
	}

	for (i = 0; i < node->child_count; i++) {
		js_serialize_html(node->children[i], buf, cap, true);
	}
}

JS_Dom *js_parse_html(const char *html) {
	JS_Dom *root = js_create_dom(NODE_ELEMENT, "html");
	JS_Dom *current = root;
	const char *page = html;
	while (*page) {
		if (*page == '<') {
			page++;
			if (*page == '!') {
				while (*page && *page != '>')
					page++;

				if (*page)
					page++;
				continue;
			}

			if (*page == '/') {
				page++;
				char tagname[64] = {0};
				int i = 0;
				while (*page && *page != '>') {
					if (i < 63)
						tagname[i++] = *page;
					page++;
				}

				if (*page)
					page++;
				if (current->parent && strcasecmp(current->tagName, tagname) == 0)
					current = current->parent;
				continue;
			}

			char tagname[64] = {0};
			int i = 0;
			while (*page && !isspace((unsigned char)*page) && *page != '>' && *page != '/') {
				if (i < 63)
					tagname[i++] = *page;
				page++;
			}

			while (current != root && js_should_autoclose(current->tagName, tagname))
				current = current->parent;

			JS_Dom *new_elem = js_create_dom(NODE_ELEMENT, tagname);
			while (*page && *page != '>' && *page != '/') {
				if (isspace((unsigned char)*page)) {
					page++;
					continue;
				}

				char key[64] = {0}, val[512] = {0};
				int k = 0;
				while (*page && *page != '=' && !isspace((unsigned char)*page) && *page != '>') {
					if (k < 63)
						key[k++] = *page;
					page++;
				}

				if (*page == '=') {
					page++;
					if (*page == '"' || *page == '\'') {
						char q = *page++;
						int v = 0;
						while (*page && *page != q) {
							if (v < 511)
								val[v++] = *page;
							page++;
						}

						if (*page)
							page++;
					} else {
						int v = 0;
						while (*page && !isspace((unsigned char)*page)
							&& *page != '>') {
							if (v < 511)
								val[v++] = *page;
							page++;
						}
					}
				}

				if (strlen(key) > 0)
					js_add_attribute(new_elem, key, val);
			}

			js_append_child(current, new_elem);
			if (*page == '/')
				page++;

			if (*page == '>')
				page++;

			if (strcasecmp(tagname, "img") != 0 && strcasecmp(tagname, "br") != 0
				&& strcasecmp(tagname, "hr") != 0 && strcasecmp(tagname, "input") != 0
				&& strcasecmp(tagname, "link") != 0)
				current = new_elem;
		} else {
			const char *text_start = page;
			while (*page && *page != '<')
				page++;

			size_t text_len = page - text_start;
			if (text_len > 0) {
				char *text_buf = calloc(1, text_len + 1);
				memcpy(text_buf, text_start, text_len);
				js_append_child(current, js_create_dom(NODE_TEXT, text_buf));
				free(text_buf);
			}
		}
	}

	return root;
}

static char *qjs_readfile(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc(sz + 1);
	if (buf) {
		fread(buf, 1, sz, f);
		buf[sz] = 0;
	}

	fclose(f);
	return buf;
}

static char *qjs_fetchfile(const char *url) {
	char host[256] = {0};
	char path[512] = {0};
	int port = 80;

	if (sscanf(url, "http://%255[^/]%511s", host, path) < 1) {
		strncpy(host, url, sizeof(host) -1);
		strcpy(path, "/");
	}

	if (strlen(path) == 0)
		strcpy(path, "/");

	struct hostent *server = gethostbyname(host);
	if (!server)
		return NULL;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};

	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		return NULL;
	}

	char req[1024] = {0};
	snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
		path, host);
	send(sockfd, req, strlen(req), 0);
	int cap = 8192, total = 0, n;
	char *resp = malloc(cap);
	while ((n = recv(sockfd, resp + total, cap - total - 1, 0)) > 0) {
		total += n;
		if (total >= cap - 1) {
			cap = 2;
			resp = realloc(resp, cap);
		}
	}

	resp[total] = '\0';
	close(sockfd);
	char *start = strstr(resp, "\r\n\r\n");
	if (start) {
		char *b = strdup(start + 4);
		free(resp);
		return b;
	}

	return resp;
}

static void *js_async_pipeline(void *arg) {
	while (g_run_pipeline) {
		JS_Task *task = NULL;
		pthread_mutex_lock(&g_app.task_mutex);
		if (g_app.task_queue) {
			task = g_app.task_queue;
			g_app.task_queue = g_app.task_queue->next;
		}
		pthread_mutex_unlock(&g_app.task_mutex);

		if (task) {
			JSContext *ctx = g_app.ctx;
			if (task->type == TASK_SETTIMEOUT) {
				usleep(task->delay * 1000);
				if (task->timeout) {
					JSValue ret = JS_Call(ctx, task->timeout, JS_UNDEFINED, 0, NULL);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, task->timeout);
				}
				free(task);
			} else if (task->url) {
				char *payload = qjs_fetchfile(task->url);
				if (payload && task->type == TASK_FETCH) {
					JSValue res_obj = JS_NewObject(ctx);
					JS_SetPropertyStr(ctx, res_obj, "text", JS_NewString(ctx, payload));
					JSValue argv = {res_obj};
					JSValue ret = JS_Call(ctx, task->resolve, JS_UNDEFINED, 1, &argv);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, task->resolve);
					JS_FreeValue(ctx, task->reject);
					free(payload);
				} else if (payload && task->type == TASK_MODULE_IMPORT) {
					JSValue mod = JS_Eval(ctx, payload, strlen(payload), task->url, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
					JSValue argv = {mod};
					JSValue ret = JS_Call(ctx, task->resolve, JS_UNDEFINED, 1, &argv);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, task->resolve);
					JS_FreeValue(ctx, task->reject);
					free(payload);
				}
				free(task->url);
				free(task);
			}

			JSContext *pctx;
			while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &pctx) > 0);
		} else {
			usleep(1);
		}
	}

	return NULL;
}

static JSValue js_global_fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	const char *url = JS_ToCString(ctx, argv[0]);
	JSValue resolving_funcs[2];
	JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

	if (JS_IsException(promise))
		return JS_EXCEPTION;

	JS_Task *task = calloc(1, sizeof(JS_Task));
	task->type = TASK_FETCH;
	task->url = strdup(url);
	task->resolve = JS_DupValue(ctx, resolving_funcs[0]);
	task->reject = JS_DupValue(ctx, resolving_funcs[1]);
	task->timeout = JS_UNDEFINED;

	pthread_mutex_lock(&g_app.task_mutex);
	task->next = g_app.task_queue;
	g_app.task_queue = task;
	pthread_mutex_unlock(&g_app.task_mutex);
	JS_FreeCString(ctx, url);
	return promise;
}

static JSValue js_global_set_timeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSValue callback = JS_DupValue(ctx, argv[0]);
	int delay = 0;
	JS_ToInt32(ctx, &delay, argv[1]);

	JS_Task *task = (JS_Task *)calloc(1, sizeof(JS_Task));
	task->type = TASK_SETTIMEOUT;
	task->timeout = callback;
	task->delay = delay;
	task->reject = JS_UNDEFINED;
	task->resolve = JS_UNDEFINED;

	pthread_mutex_lock(&g_app.task_mutex);
	task->next = g_app.task_queue;
	g_app.task_queue = task;
	pthread_mutex_unlock(&g_app.task_mutex);
	return JS_UNDEFINED;
}

static JSModuleDef *js_async_module_loader(JSContext *ctx, const char *name, void *opaque) {
	if (name && (strncmp(name, "http://", 7) == 0
		|| strncmp(name, "https://", 8) == 0)) {
		JSValue resolving_funcs[2];
		JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

		JS_Task *task = calloc(1, sizeof(JS_Task));
		task->type = TASK_MODULE_IMPORT;
		task->url = strdup(name);
		task->resolve = JS_DupValue(ctx, resolving_funcs[0]);
		task->reject = JS_DupValue(ctx, resolving_funcs[1]);

		pthread_mutex_lock(&g_app.task_mutex);
		task->next = g_app.task_queue;
		g_app.task_queue = task;
		pthread_mutex_unlock(&g_app.task_mutex);

		JSModuleDef *m = JS_NewCModule(ctx, name, NULL);
		JS_FreeValue(ctx, promise);
		return m;
	}

	return JS_NewCModule(ctx, name, NULL);
}

const char *get_attribute(JS_Dom *node, const char *key) {
	int i;
	for (i = 0; i < node->attr_count; i++) {
		if (strcasecmp(node->attributes[i].key, key) == 0)
			return node->attributes[i].value;
	}

	return NULL;
}

JSValue create_js_dom_wrapper(JSContext *ctx, JS_Dom *node) {
	if (!node)
		return JS_UNDEFINED;

	JSValue obj = JS_NewObjectClass(ctx, g_app.js_class_id);
	JS_SetOpaque(obj, node);
	return obj;
}

JS_Dom *find_node_by_id(JS_Dom *root, const char *id) {
	if (!root)
		return NULL;

	if (root->type == NODE_ELEMENT) {
		const char *val = get_attribute(root, "id");
		if (val && strcmp(val, id) == 0)
			return root;
	}

	int i;
	for (i = 0; i < root->child_count; i++) {
		JS_Dom *res = find_node_by_id(root->children[i], id);
		if (res) return res;
	}

	return NULL;
}

static JSValue js_document_get_element_by_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	const char *id = JS_ToCString(ctx, argv[0]);
	JS_Dom *found = find_node_by_id(g_app.dom_root, id);
	JS_FreeCString(ctx, id);
	if (!found)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, found);
}

static void canvas_interface_pipeline(Widget w, XtPointer client_data, XEvent *event, Boolean *cont) {
	Display *dpy = XtDisplay(w); Window win = XtWindow(w);
	XClearWindow(dpy, win);
	int y_cursor = 32 - g_app.scroll_offset_y;
	//parse_layout_pass(dpy, win, g_app.gc, g_app.dom_root, 24, &y_cursor, 740, false, false);
	pthread_mutex_lock(&g_app.canvas_mutex);
	JS_Canvas *curr = g_app.canvas_queue;
	while (curr) {
		if (curr->type == CMD_RECT) {
			XFillRectangle(dpy, win, g_app.gc, curr->x, curr->y, curr->w, curr->h);
			XCopyArea(dpy, g_app.canvas_buffer, XtWindow(g_app.render_canvas),
				g_app.gc, 0, 0, g_app.canvas_w, g_app.canvas_h, 0, 0);
		}
		curr = curr->next;
	}
	pthread_mutex_unlock(&g_app.canvas_mutex);
}

static JSValue js_canvas_fill_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc >= 4) {
		int x = 0, y = 0, w = 0, h = 0;

		JS_ToInt32(ctx, &x, argv[0]);
		JS_ToInt32(ctx, &y, argv[1]);
		JS_ToInt32(ctx, &w, argv[2]);
		JS_ToInt32(ctx, &h, argv[3]);
		pthread_mutex_lock(&g_app.canvas_mutex);
		JS_Canvas *cmd = calloc(1, sizeof(JS_Canvas));
		cmd->type = CMD_RECT;
		cmd->x = x;
		cmd->y = y;
		cmd->w = w;
		cmd->h = h;
		cmd->next = g_app.canvas_queue;
		g_app.canvas_queue = cmd;
		pthread_mutex_unlock(&g_app.canvas_mutex);
	}

	return JS_UNDEFINED;
}

static JSValue js_canvas_get_context(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSValue ctx_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, ctx_obj, "fillRect", JS_NewCFunction(ctx, js_canvas_fill_rect, "fillRect", 4));
	return ctx_obj;
}

static JSValue js_dom_get_prop_expanded(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv, int magic) {
	JS_Dom *n = JS_GetOpaque(this_val, g_app.js_class_id);
	int i;
	if (!n)
		return JS_UNDEFINED;

	switch (magic) {
		case 2:
			return JS_NewString(ctx, get_attribute(n, "id") ? get_attribute(n, "id") : ""); // id
		case 3:
			return JS_NewString(ctx, get_attribute(n, "class") ? get_attribute(n, "class") : ""); // className
		case 4: { // innerHTML
				size_t cap = 4096;
				char *buf = calloc(1, cap);
				//serialize_to_html(n, &buf, &cap, false);
				JSValue val = JS_NewString(ctx, buf);
				free(buf);
				return val;
			}
		case 5: { // outerHTML
				size_t cap = 4096;
				char *buf = calloc(1, cap);
				//serialize_to_html(n, &buf, &cap, true);
				JSValue val = JS_NewString(ctx, buf);
				free(buf);
				return val;
			}
		case 6:
			return n->parent ? create_js_dom_wrapper(ctx, n->parent) : JS_NULL; // parentNode
		case 7: { // childNodes Array structure mapping
				JSValue arr = JS_NewArray(ctx);
				for (i = 0; i < n->child_count; i++) {
					JS_SetPropertyUint32(ctx, arr, i, create_js_dom_wrapper(ctx, n->children[i]));
				}
				return arr;
			}
		case 8:
			return (n->child_count > 0) ? create_js_dom_wrapper(ctx, n->children[0]) : JS_NULL; // firstChild
		case 9:
			return (n->child_count > 0) ? create_js_dom_wrapper(ctx, n->children[n->child_count - 1]) : JS_NULL; // lastChild
		case 10: { // nextSibling
				if (n->parent) {
					for (i = 0; i < n->parent->child_count - 1; i++) {
						if (n->parent->children[i] == n)
							return create_js_dom_wrapper(ctx, n->parent->children[i + 1]);
					}
				}
				return JS_NULL;
			}
		case 11: { // previousSibling
				if (n->parent) {
					for (i = 1; i < n->parent->child_count; i++) {
						if (n->parent->children[i] == n)
							return create_js_dom_wrapper(ctx, n->parent->children[i - 1]);
					}
				}
				return JS_NULL;
			}
		case 12: // length
			return JS_NewInt32(ctx, n->child_count);
		case 13: // nodeType
			return JS_NewInt32(ctx, (int)n->type);
	}

	return JS_UNDEFINED;
}

static JSValue js_dom_set_prop_expanded(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv, int magic) {
	JS_Dom *n = JS_GetOpaque(this_val, g_app.js_class_id);
	if (!n)
		return JS_UNDEFINED;

	const char *str = JS_ToCString(ctx, argv[0]);
	int i;
	if (magic == 2) { // id
		js_add_attribute(n, "id", str);
	} else if (magic == 3) { // className
		js_add_attribute(n, "class", str);
	} else if (magic == 4) { // innerHTML Dynamic sub-tree clear and re-parse execution
		for (i = 0; i < n->child_count; i++)
			js_free_dom(n->children[i]);
		n->child_count = 0;
		JS_Dom *new_subtree = js_parse_html(str);
		for (i = 0; i < new_subtree->child_count; i++) {
			js_append_child(n, new_subtree->children[i]);
		}

		new_subtree->child_count = 0;
		js_free_dom(new_subtree);
	}

	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_dom_add_eventlistener(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	JS_Dom *node = JS_GetOpaque(this_val, g_app.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	const char *type = JS_ToCString(ctx, argv[0]);
	if (type) {
		const char *id_val = get_attribute(node, "id");
		JS_Listener *listener = calloc(1, sizeof(JS_Listener));
		listener->type = strdup(type);
		listener->id = strdup(id_val);
		listener->callback = JS_DupValue(ctx, argv[1]);
		listener->next = node->listeners;
		node->listeners = listener;

		JS_FreeCString(ctx, type);
	}

	return JS_UNDEFINED;
}

static JSValue js_dom_dispatch_event(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	JS_Dom *n = JS_GetOpaque(this_val, g_app.js_class_id);
	if (n && argc >= 1) {
		const char *id_event = JS_ToCString(ctx, argv[0]);
		if (id_event) {
			const char *id_val = get_attribute(n, "id");
			if (strcmp(id_event, id_val) == 0) {
				const char *id_type = JS_ToCString(ctx, argv[1]);
				if (id_type) {
					JS_Listener *curr = n->listeners;
					while (curr) {
						if (strcmp(curr->type, id_type) == 0
							&& strcmp(curr->id, id_val) == 0) {
							JSValue ret = JS_Call(ctx, curr->callback, this_val, argc, argv);
							JS_FreeValue(ctx, ret);
							break;
						}
						curr = curr->next;
					}
					JS_FreeCString(ctx, id_type);
				}
			}
			JS_FreeCString(ctx, id_event);
		}
	}

	return JS_TRUE;
}

static JSValue js_dom_get_children(JSContext *ctx, JSValue this_val,
	int argc, JSValue *argv) {
	JS_Dom *node = JS_GetOpaque(this_val, g_app.js_class_id);
	if (node) {
		JSValue arr = JS_NewArray(ctx);
		int i;
		for (i = 0; i < node->child_count; i++) {
			JS_SetPropertyUint32(ctx, arr, i, create_js_dom_wrapper(ctx, node->children[i]));
		}

		return arr;
	}

	return JS_UNDEFINED;
}

static void js_parse_stylesheet(const char *css_content) {
	const char *ptr = css_content;
	while (*ptr) {
		if (*ptr == '-' && *(ptr + 1) == '-') {
			const char *name_start = ptr;
			while (*ptr && *ptr != ':' && !isspace((unsigned char)*ptr))
				ptr++;

			size_t name_len = ptr - name_start;
			char *var_name = calloc(1, name_len + 1);
			memcpy(var_name, name_start, name_len);
			while (*ptr && (*ptr == ':' || isspace((unsigned char)*ptr)))
				ptr++;

			const char *val_start = ptr;
			while (*ptr && *ptr != ';' && *ptr != '}')
				ptr++;

			size_t val_len = ptr - val_start;
			char *var_val = calloc(1, val_len + 1);
			memcpy(var_val, val_start, val_len);
			JS_CSSVariable *s_node = calloc(1, sizeof(JS_CSSVariable));
			s_node->name = var_name;
			s_node->value = var_val;
			s_node->next = g_app.style_vars;
			g_app.style_vars = s_node;
			ptr += val_len;
		} else {
			ptr++;
		}
	}
}

static void run_javascript(JSContext *ctx, const char *script, int is_mod) {
	if (!script)
		return;

	JSValue v = JS_Eval(ctx, script, strlen(script), "",
		(is_mod ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL));
	js_std_loop(ctx);
	if (JS_IsException(v)) {
		JSValue ex = JS_GetException(ctx);
		const char *msg = JS_ToCString(ctx, ex);
		fprintf(stderr, "Runtime Exception: %s\n", msg);
		JS_FreeCString(ctx, msg);
		JS_FreeValue(ctx, ex);
	}

	JS_FreeValue(ctx, v);
}

static void execute_inline_js(JSContext *ctx, JS_Dom *node) {
	if (!node)
		return;

	int i;
	if (node->type == NODE_ELEMENT) {
		for (i = 0; i < EVENT_CHANGE; i++) {
			const char *event_type = LEGACY_EVENT_NAMES[i];
			const char *inline_event = get_attribute(node, event_type);
			if (inline_event) {
				char js_buf[4096] = {0};
				if (strcmp(event_type, "onclick") == 0)
					snprintf(js_buf, sizeof(js_buf) - 1, "(function(){ %s })", inline_event);

				JSValue eval_func = (strcmp(event_type, "onclick") == 0)
					? JS_Eval(ctx, js_buf, strlen(js_buf), "<inline_click>", JS_EVAL_TYPE_GLOBAL)
					: JS_Eval(ctx, inline_event, strlen(inline_event), "<legacy_event>", JS_EVAL_TYPE_GLOBAL);

				if (JS_IsFunction(ctx, eval_func) && strcmp(event_type, "onclick") == 0) {
					node->js_onclick = eval_func;
				}
			}
		}

		for (i = 0; i < node->child_count; i++)
			execute_inline_js(ctx, node->children[i]);
	}
}

static void execute_script_blocks(JSContext *ctx, JS_Dom *n) {
	if (!n)
		return;

	int i;
	if (n->type == NODE_ELEMENT) {
		if (strcasecmp(n->tagName, "link") == 0) {
			char *href = NULL;
			int is_css = 0;
			for (i = 0; i < n->attr_count; i++) {
				if (strcasecmp(n->attributes[i].key, "rel") == 0
					&& strcasecmp(n->attributes[i].value, "stylesheet") == 0)
					is_css = 1;
				else if (strcasecmp(n->attributes[i].key, "href") == 0)
					href = n->attributes[i].value;
			}

			if (is_css && href) {
				char *css_data = qjs_readfile(href);
				if (css_data) {
					js_parse_stylesheet(css_data);
					free(css_data);
				}
			}
		} else if (strcasecmp(n->tagName, "script") == 0) {
			int is_mod = 0;
			char *src_path = NULL;
			for (i = 0; i < n->attr_count; i++) {
				if (strcasecmp(n->attributes[i].key, "type") == 0
					&& strcasecmp(n->attributes[i].value, "module") == 0)
					is_mod = 1;
				else if (strcasecmp(n->attributes[i].key, "src") == 0)
					src_path = n->attributes[i].value;
			}

			if (src_path) {
				char *content = (strncmp(src_path, "http", 4) == 0)
					? qjs_fetchfile(src_path)
					: qjs_readfile(src_path);
				if (content) {
					run_javascript(ctx, content, is_mod);
					free(content);
				}
			} else {
				for (i = 0; i < n->child_count; i++) {
					if (n->children[i]->type == NODE_TEXT)
						run_javascript(ctx, n->children[i]->textContent, is_mod);
				}
			}
		}

		for (i = 0; i < n->child_count; i++)
			execute_script_blocks(ctx, n->children[i]);
	}
}

static JS_Dom *find_clicked_node(JS_Dom *node, int mx, int my) {
	if (!node)
		return NULL;

	int i, ay = node->y - g_app.scroll_offset_y;
	if (mx >= node->x && mx <= node->x + node->width && my >= ay && my <= ay + node->height) {
		for (i = 0; i < node->child_count; i++) {
			JS_Dom *found = find_clicked_node(node->children[i], mx, my);
			if (found)
				return found;
		}

		return node;
	}

	return NULL;
}

void qjs_click_event(Widget w, XtPointer data, XEvent *ev, Boolean *cont) {
	if (ev->type == ButtonPress && ev->xbutton.button == Button1) {
		int i, mx = ev->xbutton.x, my = ev->xbutton.y;
		JS_Dom *target = find_clicked_node(g_app.dom_root, mx, my);
		if (target) {
			JSValue mouse_event = JS_NewObject(g_app.ctx);
			JS_SetPropertyStr(g_app.ctx, mouse_event, "type", JS_NewString(g_app.ctx, "click"));
			JS_SetPropertyStr(g_app.ctx, mouse_event, "clientX", JS_NewInt32(g_app.ctx, mx));
			JS_SetPropertyStr(g_app.ctx, mouse_event, "clientY", JS_NewInt32(g_app.ctx, my));
			const char *id_val = get_attribute(target, "id");
			JS_Listener *curr = target->listeners;
			while (curr) {
				if (strcmp(curr->type, "click") == 0 && strcmp(curr->id, id_val) == 0) {
					JSValue ret = JS_Call(g_app.ctx, curr->callback, JS_UNDEFINED, 1, &mouse_event);
					JS_FreeValue(g_app.ctx, ret);
					break;
				}
				curr = curr->next;
			}

			JS_FreeValue(g_app.ctx, mouse_event);
			if (!JS_IsUndefined(target->js_onclick)) {
				JS_FreeValue(g_app.ctx, JS_Call(g_app.ctx, target->js_onclick, JS_UNDEFINED, 0, NULL));
			}
		}
	}
}

#ifdef USE_QUICKJS_NG
static FORCEINLINE void *my_malloc(void *opaque, size_t size) {
	return malloc(size);
}

static FORCEINLINE void my_free(void *opaque, void *ptr) {
	free(ptr);
}

static FORCEINLINE void *my_realloc(void *opaque, void *ptr, size_t size) {
	return realloc(ptr, size);
}

static FORCEINLINE void *my_calloc(void *opaque, size_t count, size_t size) {
	return calloc(count, size);
}

static FORCEINLINE size_t my_malloc_usable_size(const void *ptr) {
	return malloc_usable_size((void *)ptr); // Platform-specific implementation needed
}

static JSMallocFunctions mf = {
	.js_calloc = my_calloc,
	.js_malloc = my_malloc,
	.js_free = my_free,
	.js_realloc = my_realloc,
	.js_malloc_usable_size = my_malloc_usable_size
};
#else
static FORCEINLINE void *my_malloc(JSMallocState *opaque, size_t size) {
	return malloc(size);
}

static FORCEINLINE void my_free(JSMallocState *opaque, void *ptr) {
	free(ptr);
}

static FORCEINLINE void *my_realloc(JSMallocState *opaque, void *ptr, size_t size) {
	return realloc(ptr, size);
}

static FORCEINLINE size_t my_malloc_usable_size(const void *ptr) {
	return malloc_usable_size((void *)ptr); // Platform-specific implementation needed
}

static JSMallocFunctions mf = {
	.js_malloc = my_malloc,
	.js_free = my_free,
	.js_realloc = my_realloc,
	.js_malloc_usable_size = my_malloc_usable_size
};

FORCEINLINE int JS_ToBigUint64(JSContext *ctx, size_t *pbyte_length, JSValueConst obj) {
	return JS_ToBigInt64(ctx, (int64_t *)pbyte_length, obj);
}

uint8_t *JS_GetUint8Array(JSContext *ctx, size_t *pbyte_length, JSValueConst obj) {
	size_t byte_offset = 0;
	size_t byte_length = 0;
	size_t bytes_per_element = 0;
	JSValue abuf = JS_GetTypedArrayBuffer(ctx, obj, &byte_offset, &byte_length, &bytes_per_element);

	if (JS_IsException(abuf)) {
		if (JS_IsArrayBuffer(obj)) {
			abuf = JS_DupValue(ctx, obj);
			byte_offset = 0;
		} else {
			return NULL;
		}
	}

	size_t abuf_len = 0;
	uint8_t *ptr = JS_GetArrayBuffer(ctx, &abuf_len, abuf);
	JS_FreeValue(ctx, abuf);

	if (!ptr) return NULL;
	if (pbyte_length) *pbyte_length = byte_length;
	return ptr + byte_offset;
}
#endif

static void js_object_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
	JS_Dom *data = JS_GetOpaque(val, g_app.js_class_id);
	if (!data)
		return;

	JS_MarkValue(rt, data->js_object, mark_func);
	JS_MarkValue(rt, data->js_onclick, mark_func);

	JS_Listener *curr = data->listeners;
	while (curr != NULL) {
		JS_Listener *next = curr->next;
		JS_MarkValue(rt, curr->callback, mark_func);
		curr = next;
	}
}

static void js_object_finalizer(JSRuntime *rt, JSValue val) {
	JS_Dom *data = JS_GetOpaque(val, g_app.js_class_id);
	if (!data)
		return;

	JS_FreeValueRT(rt, data->js_object);
	JS_FreeValueRT(rt, data->js_onclick);

	JS_Listener *curr = data->listeners;
	while (curr != NULL) {
		JS_Listener *next = curr->next;
		JS_FreeValueRT(rt, curr->callback);
		js_free_rt(rt, curr);
		curr = next;
	}

	js_free_rt(rt, data);
}

static JSClassDef js_dom_obs_class = {"MutationObserver"};
static JSClassDef js_dom_node_class = { "JS_Dom",
	.finalizer = js_object_finalizer,
	.gc_mark = js_object_mark,
};

static void push_log(bow_t web, const char *text) {
	size_t text_len = strlen(text);
	size_t current_len = !web->qjs->js_buf ? 0 : strlen(web->qjs->js_buf);

	if (current_len + text_len >= web->qjs->js_buf_size) {
		web->qjs->js_buf_size += text_len + 1024;
		web->qjs->js_buf = realloc(web->qjs->js_buf, web->qjs->js_buf_size);
	}

	strcat(web->qjs->js_buf, text);
}

/* pipe JS console output to buffer */
static JSValue qjs_console_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	bow_t web = (bow_t)JS_GetContextOpaque(ctx);
	int i;
	for (i = 0; i < argc; i++) {
		size_t len;
		const char *str = JS_ToCStringLen(ctx, &len, argv[i]);
		if (str) {
			push_log(web, str);
			if (i != argc - 1)
				push_log(web, " ");
			JS_FreeCString(ctx, str);
		}
	}
	push_log(web, "\n");
	return JS_UNDEFINED;
}

static JSValue js_native_system_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	int i;

	for (i = 0; i < argc; i++) {
		const char *str = JS_ToCString(ctx, argv[i]);
		if (str) {
			fprintf(stdout, "%s\n", str);
			JS_FreeCString(ctx, str);
		}
	}

	return JS_UNDEFINED;
}

static JSValue js_observer_observe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	JS_Observer *obs = JS_GetOpaque(this_val, g_app.observer_class_id);
	JS_Dom *target = JS_GetOpaque(argv[0], g_app.js_class_id);
	if (obs && target) {
		obs->target = target;
	}

	return JS_UNDEFINED;
}

static JSValue js_mutation_observer_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	JSValue js_obj = JS_NewObjectClass(ctx, g_app.observer_class_id);
	JS_Observer *obs = calloc(1, sizeof(JS_Observer));
	obs->callback = JS_DupValue(ctx, argv[0]);
	obs->target = NULL;
	obs->next = g_app.observers;
	g_app.observers = obs;
	JS_SetOpaque(js_obj, obs);
	return js_obj;
}

static JSValue js_dom_append_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	JS_Dom *parent = JS_GetOpaque(this_val, g_app.js_class_id);
	JS_Dom *child = JS_GetOpaque(argv[0], g_app.js_class_id);
	if (parent && child) {
		js_append_child(parent, child);
		notify_mutation("childList", parent);
	}

	return JS_DupValue(ctx, argv[0]);
}

static JSValue js_dom_set_attribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	JS_Dom *node = JS_GetOpaque(this_val, g_app.js_class_id);
	if (node) {
		const char *k = JS_ToCString(ctx, argv[0]);
		const char *v = JS_ToCString(ctx, argv[1]);
		if (k && v) {
			js_add_attribute(node, k, v);
			notify_mutation("attributes", node);
			JS_FreeCString(ctx, k);
			JS_FreeCString(ctx, v);
		}
	}

	return JS_UNDEFINED;
}

static JSValue js_dom_get_property(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
	JS_Dom *node = JS_GetOpaque(this_val, g_app.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	if (magic == 0)
		return JS_NewString(ctx, node->tagName ? node->tagName : "");
	else if (magic == 1)
		return JS_NewString(ctx, node->textContent ? node->textContent : "");

	return JS_UNDEFINED;
}

static JSValue js_dom_set_text_content(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic) {
	JS_Dom *node = JS_GetOpaque(this_val, g_app.js_class_id);
	if (node) {
		const char *str = JS_ToCString(ctx, argv[0]);
		if (str) {
			if (node->type == NODE_TEXT) {
				node->textContent = strdup(str);
			} else {
				JS_Dom *tc = js_create_dom(NODE_TEXT, str);
				if (tc)
					js_append_child(node, tc);
			}
			notify_mutation("characterData", node);
			JS_FreeCString(ctx, str);
		}
	}
	return JS_UNDEFINED;
}

static JSValue js_document_create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	const char *tag = JS_ToCString(ctx, argv[0]);
	JS_Dom *native = js_create_dom(NODE_ELEMENT, tag);
	JS_FreeCString(ctx, tag);
	return create_js_dom_wrapper(ctx, native);
}

FORCEINLINE bool qjs_is_running(bow_t web) {
	return web && web->qjs && web->qjs->ctx;
}

FORCEINLINE char *qjs_get_buf(bow_t web, unsigned long *size) {
	*size = web->qjs->js_buf_size;
	return web->qjs->js_buf;
}

FORCEINLINE void qjs_set_buf(bow_t web, char *data, unsigned long size) {
	if (web && web->qjs) {
		web->qjs->js_buf_size = size;
		web->qjs->js_buf = data;
	}
}

FORCEINLINE void qjs_free_buf(bow_t web) {
	if (web && web->qjs && web->qjs->js_buf) {
		free(web->qjs->js_buf);
		web->qjs->js_buf_size = 0;
		web->qjs->js_buf = NULL;
	}
}

static void qjs_proto_init(JSContext *ctx, JSRuntime *rt) {
	JS_NewClassID(&g_app.js_class_id);
	JS_NewClass(rt, g_app.js_class_id, &js_dom_node_class);

	JS_NewClassID(&g_app.observer_class_id);
	JS_NewClass(rt, g_app.observer_class_id, &js_dom_obs_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, proto, "appendChild",
		JS_NewCFunction(ctx, js_dom_append_child, "get_appendChild", 1));

	JS_SetPropertyStr(ctx, proto, "setAttribute",
		JS_NewCFunction(ctx, js_dom_set_attribute, "get_setAttribute", 2));

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "textContent"),
		JS_NewCFunctionMagic(ctx, js_dom_get_property, "get_textContent", 0, JS_CFUNC_getter, 1),
		JS_NewCFunctionMagic(ctx, js_dom_set_text_content, "set_textContent", 1, JS_CFUNC_setter, 1), 0);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "tagName"),
		JS_NewCFunctionMagic(ctx, js_dom_get_property, "get_tagName", 0, JS_CFUNC_getter_magic, 0),
		JS_UNDEFINED, JS_PROP_HAS_GET);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "id"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_id", 1, JS_CFUNC_getter_magic, 2),
		JS_NewCFunctionMagic(ctx, js_dom_set_prop_expanded, "set_id", 1, JS_CFUNC_setter_magic, 2),
		JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "className"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_className", 1, JS_CFUNC_getter_magic, 3),
		JS_NewCFunctionMagic(ctx, js_dom_set_prop_expanded, "set_className", 1, JS_CFUNC_setter_magic, 3),
		JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "innerHTML"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_innerHTML", 1, JS_CFUNC_getter_magic, 4),
		JS_NewCFunctionMagic(ctx, js_dom_set_prop_expanded, "set_innerHTML", 1, JS_CFUNC_setter_magic, 4),
		JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "outerHTML"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_outerHTML", 1, JS_CFUNC_getter_magic, 5),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "parentNode"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_parentNode", 0, JS_CFUNC_getter_magic, 6),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "childNodes"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_childNodes", 0, JS_CFUNC_getter_magic, 7),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "firstChild"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_firstChild", 0, JS_CFUNC_getter_magic, 8),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "lastChild"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_lastChild", 0, JS_CFUNC_getter_magic, 9),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "nextSibling"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_nextSibling", 0, JS_CFUNC_getter_magic, 10),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "previousSibling"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_previousSibling", 0, JS_CFUNC_getter_magic, 11),
		JS_UNDEFINED, JS_PROP_CONFIGURABLE);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "length"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_length", 0, JS_CFUNC_getter_magic, 12),
		JS_UNDEFINED, JS_PROP_HAS_GET);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "nodeType"),
		JS_NewCFunctionMagic(ctx, js_dom_get_prop_expanded, "get_nodeType", 0, JS_CFUNC_getter_magic, 13),
		JS_UNDEFINED, JS_PROP_HAS_GET);

	JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, "children"),
		JS_NewCFunction(ctx, (JSCFunction *)js_dom_get_children, "get_children", 0),
		JS_UNDEFINED, JS_PROP_HAS_GET);

	JS_DefinePropertyValueStr(ctx, proto, "addEventListener",
		JS_NewCFunction(ctx, js_dom_add_eventlistener, "addEventListener", 2),
		JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

	JS_SetPropertyStr(ctx, proto, "dispatchEvent",
		JS_NewCFunction(ctx, js_dom_dispatch_event, "dispatchEvent", 2));
	JS_SetClassProto(ctx, g_app.js_class_id, proto);
}

static void qjs_interfaces_set(JSContext *ctx, JS_Dom *root) {
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue doc_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, doc_obj, "body", create_js_dom_wrapper(ctx, root));
	JS_SetPropertyStr(ctx, doc_obj, "getElementById",
		JS_NewCFunction(ctx, js_document_get_element_by_id, "getElementById", 1));

	JS_SetPropertyStr(ctx, doc_obj, "createElement",
		JS_NewCFunction(ctx, js_document_create_element, "createElement", 1));

	JS_SetPropertyStr(ctx, global, "document", doc_obj);
	JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

	JS_SetPropertyStr(ctx, global, "fetch",
		JS_NewCFunction(ctx, js_global_fetch, "fetch", 1));

	JS_SetPropertyStr(ctx, global, "setTimeout",
		JS_NewCFunction(ctx, js_global_set_timeout, "setTimeout", 2));

	JSValue canvas = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, canvas, "fillRect",
		JS_NewCFunction(ctx, js_canvas_fill_rect, "fillRect", 4));
	JS_SetPropertyStr(ctx, canvas, "getContext",
		JS_NewCFunction(ctx, js_canvas_get_context, "getContext", 1));
	JS_SetPropertyStr(ctx, global, "canvas", canvas);
/*
	JSValue console_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, console_obj, "log",
		JS_NewCFunction(ctx, js_native_system_log, "log", 1));
	JS_SetPropertyStr(ctx, global, "console", console_obj);
*/
	JSValue obs_proto = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obs_proto, "observe",
		JS_NewCFunction(ctx, js_observer_observe, "observe", 2));

	JSValue obs_ctor = JS_NewCFunction2(ctx, js_mutation_observer_ctor,
		"MutationObserver", 1, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, obs_ctor, obs_proto);
	JS_SetPropertyStr(ctx, global, "MutationObserver", obs_ctor);

	JSValue extra_config = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, extra_config, "engine", JS_NewString(ctx, "Bow 0.6.0-QuickJS"));
	JS_SetPropertyStr(ctx, global, "sysConfig", extra_config);
	g_app.js_obs = obs_proto;
	JS_FreeValue(ctx, global);
}

/* Create QuickJS interpreter state */
bool qjs_init(bow_t web) {
	if (!web)
		return false;

	if (web->qjs && web->qjs->ctx)
		qjs_free(web);

	if (!web->qjs) {
		web->qjs = &g_app;
		if (!web->ats) {
			XInitThreads();
			XtToolkitThreadInitialize();
			XrmInitialize();
		}

		pthread_t thread;
		pthread_mutex_init(&g_app.task_mutex, NULL);
		pthread_create(&thread, NULL, js_async_pipeline, NULL);
		pthread_detach(thread);
	}

	web->qjs->rt = JS_NewRuntime();
	if (!web->qjs->rt) {
		g_run_pipeline = false;
		webview_debug("%s", "Failed to create QuickJS runtime.");
		web->qjs = NULL;
		return false;
	}

	js_std_init_handlers(web->qjs->rt);
	JS_SetModuleLoaderFunc(web->qjs->rt, NULL, (JSModuleLoaderFunc *)&js_async_module_loader, NULL);

	web->qjs->ctx = JS_NewContext(web->qjs->rt);
	if (!web->qjs->ctx) {
		g_run_pipeline = false;
		webview_debug("%s", "Failed to create QuickJS context.");
		js_std_free_handlers(web->qjs->rt);
		JS_FreeRuntime(web->qjs->rt);
		web->qjs->rt = NULL;
		web->qjs = NULL;
		return false;
	}

	js_init_module_std(web->qjs->ctx, "std");
	js_init_module_os(web->qjs->ctx, "os");
	js_std_add_helpers(web->qjs->ctx, 0, NULL);

	/* Add `bow_t` instance */
	JS_SetContextOpaque(web->qjs->ctx, web);
	web->qjs->js_obs = JS_UNDEFINED;
	web->qjs->js_this = JS_NewObject(web->qjs->ctx);
	if (JS_IsException(web->qjs->js_this)) {
		g_run_pipeline = false;
		js_std_free_handlers(web->qjs->rt);
		JS_FreeContext(web->qjs->ctx);
		JS_FreeRuntime(web->qjs->rt);
		web->qjs->rt = NULL;
		web->qjs->ctx = NULL;
		web->qjs = NULL;
		return false;
	}

	g_app.dom_root = js_create_dom(NODE_ELEMENT, "body");
	qjs_proto_init(web->qjs->ctx, web->qjs->rt);
	qjs_interfaces_set(web->qjs->ctx, g_app.dom_root);
	return true;
}

bool qjs_hook(bow_t web, JSValue this, void *func, const char *name, int args) {
	JSValue v_func = JS_NewCFunction(web->qjs->ctx, (JSCFunction *)func, name, args);
	if (JS_IsException(v_func)
		|| JS_SetPropertyStr(web->qjs->ctx, this, name, v_func) < 0) {
		JS_FreeValue(web->qjs->ctx, v_func);
		return false;
	}

	return true;
}

bool qjs_hook_set(bow_t web, JSValue this, const char *namespace) {
	JSValue gl = JS_GetGlobalObject(web->qjs->ctx);
	bool status = true;
	if (JS_SetPropertyStr(web->qjs->ctx, gl, namespace, this) < 0)
		status = false;

	JS_FreeValue(web->qjs->ctx, gl);
	return status;
}

FORCEINLINE bool qjs_func(bow_t web, void *func, const char *name, int args) {
	return qjs_hook(web, web->qjs->js_this, func, name, args);
}

FORCEINLINE bool qjs_func_set(bow_t web, const char *namespace) {
	bool r = qjs_hook_set(web, web->qjs->js_this, namespace);
	web->qjs->js_this = JS_UNDEFINED;
	return r;
}

FORCEINLINE char *qjs_get_file(const char *path_url) {
	if (!path_url || strlen(path_url) == 0)
		return NULL;

	return (strncmp(path_url, "http", 4) == 0) ? qjs_fetchfile(path_url) : qjs_readfile(path_url);
}

FORCEINLINE JS_Dom *qjs_parse(bow_t web, const char *html) {
	JS_Dom *parsed = js_parse_html(html);
	js_append_child(web->qjs->dom_root, parsed);
	if (web->js_data)
		free(web->js_data);

	web->js_data = (void *)html;
	return parsed;
}

bool qjs_is_module(const char *filepath) {
	size_t len = filepath ? strlen(filepath) : 0;
	bool is_mod = false;
	if (len == 0 || len <= 3)
		return is_mod;

	if (len >= 4 && strcmp(&filepath[len - 4], ".mjs") == 0)
		is_mod = true;

	return is_mod;
}

FORCEINLINE void qjs_run_script(bow_t web, JS_Dom *parsed, const char *js, bool is_module) {
	if (!parsed && js) {
		run_javascript(web->qjs->ctx, js, is_module);
	} else if (parsed) {
		execute_inline_js(web->qjs->ctx, parsed);
		execute_script_blocks(web->qjs->ctx, parsed);
	}
}

/* Destroy QuickJS interpreter state */
void qjs_free(bow_t web) {
	JS_FreeValue(web->qjs->ctx, web->qjs->js_obs);
	JS_FreeValue(web->qjs->ctx, web->qjs->js_this);
	js_std_free_handlers(web->qjs->rt);
	JS_FreeContext(web->qjs->ctx);
	JS_FreeRuntime(web->qjs->rt);
	web->qjs->ctx = NULL;
	web->qjs->rt = NULL;
}

void qjs_exec(bow_t web, ats_str script, bool is_file) {
	if (!qjs_init(web))
		return;

	ats_str file = is_file ? qjs_get_file(script) : script;
	if (file) {
		int r = 0;
		JSValue result = JS_Eval(web->qjs->ctx, file, strlen(file),
			"<input>", JS_EVAL_TYPE_MODULE | JS_EVAL_TYPE_GLOBAL);
#ifdef USE_QUICKJS_NG
		int r = js_std_loop(web->qjs->ctx);
#else
		js_std_loop(web->qjs->ctx);
#endif
		if (JS_IsException(result) || r) {
			js_std_dump_error(web->qjs->ctx);
		}
#ifdef USE_DEBUG
		JSMemoryUsage stats;
		JS_ComputeMemoryUsage(web->qjs->rt, &stats);
		JS_DumpMemoryUsage(stdout, &stats, web->qjs->rt);
#endif
		if (is_file)
			free(file);
		JS_FreeValue(web->qjs->ctx, result);
	}

exec_quickjs_finished:
	qjs_free(web);
}
