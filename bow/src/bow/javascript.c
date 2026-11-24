#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include "javascriptP.h"

static Bow_QJS gl_ctx = {0};
static volatile bool g_run_pipeline = true;
static jsMessage_cb g_js_msg_cb = NULL;
static void *g_js_msg_closure = NULL;

/* Forward declarations */
static void define_accessor(JSContext *ctx, JSValue proto, const char *name, JSCFunction *getter, JSCFunction *setter);
static JSValue create_js_dom_wrapper(JSContext *ctx, HtmlDom_t *node);
static JSValue create_js_nodelist(JSContext *ctx, HtmlDom_t **items, size_t count);
static void notify_mutation(const char *type, HtmlDom_t *target);
static void execute_script_blocks(JSContext *ctx, HtmlDom_t *n);
static void run_javascript(JSContext *ctx, const char *script, int is_mod);
static void qjs_free_timers(JSContext *ctx);
static void js_free_dom(HtmlDom_t *node);
static JSValue js_canvas_get_context(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_getBoundingRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* HTMLCanvasElement width & height getters/setters */
static JSValue js_canvas_get_width(JSContext *ctx, JSValueConst this_val);
static JSValue js_canvas_set_width(JSContext *ctx, JSValueConst this_val, JSValueConst val);
static JSValue js_canvas_get_height(JSContext *ctx, JSValueConst this_val);
static JSValue js_canvas_set_height(JSContext *ctx, JSValueConst this_val, JSValueConst val);
// Create CanvasRenderingContext2D object
static JSValue create_canvas_context(JSContext *ctx, JSValue obj);
extern void HTMLCanvasResize(int w, int h);
extern void HTMLCanvasClearRect(int x, int y, int w, int h);
extern void HTMLCanvasStrokeSegments(XSegment *segs, int nsegs, const char *color_str);
extern void HTMLCanvasFlush(void);
extern void HTMLCanvasGetBounds(int *x, int *y, int *w, int *h);

void bow_jsMessage_set(jsMessage_cb cb, void *closure) {
	g_js_msg_cb = cb;
	g_js_msg_closure = closure;
}

static JSValue js_bow_post_message(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	const char *handler_name;
	const char *payload = NULL;
	JSValue json_val = JS_UNDEFINED;

	(void)this_val;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "bow.postMessage requires a handler name");

	handler_name = JS_ToCString(ctx, argv[0]);
	if (!handler_name)
		return JS_EXCEPTION;

	if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
		if (JS_IsString(argv[1])) {
			payload = JS_ToCString(ctx, argv[1]);
		} else {
			json_val = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
			if (!JS_IsException(json_val))
				payload = JS_ToCString(ctx, json_val);
		}
	}

	if (g_js_msg_cb != NULL) {
		g_js_msg_cb(handler_name, payload ? payload : "", g_js_msg_closure);
	}

	if (payload)
		JS_FreeCString(ctx, payload);
	if (!JS_IsUndefined(json_val))
		JS_FreeValue(ctx, json_val);
	JS_FreeCString(ctx, handler_name);

	return JS_UNDEFINED;
}

/* Bind an externally parsed HtmlDom_t tree as the active document root */
void qjs_bind_dom(bow_t web, HtmlDom_t *dom) {
	if (!web || !web->qjs || !web->qjs->ctx || !dom)
		return;

	if (web->qjs->dom_root && web->qjs->dom_root != dom) {
		js_free_dom(web->qjs->dom_root);
	}
	web->qjs->dom_root = dom;

	JSValue global = JS_GetGlobalObject(web->qjs->ctx);
	JSValue doc_obj = JS_GetPropertyStr(web->qjs->ctx, global, "document");
	if (JS_IsObject(doc_obj)) {
		JS_SetOpaque(doc_obj, dom);
	}

	JS_FreeValue(web->qjs->ctx, doc_obj);
	JS_FreeValue(web->qjs->ctx, global);
}

static unsigned long long hash_dom_snapshot(bow_t web) {
	char *snap = bow_engine_get_dom_snapshot(web);
	unsigned long long h = 14695981039346656037ULL;
	size_t i;

	if (!snap)
		return 0;

	size_t len = strlen(snap);
	for (i = 0; i < len; i++) {
		h ^= (unsigned char)snap[i];
		h *= 1099511628211ULL;
	}
	free(snap);
	return h;
}

void bow_reEval_js(bow_t web, const char *script) {
	unsigned long long before_hash;
	unsigned long long after_hash;

	if (!web || !script || !*script)
		return;

	if (!qjs_is_running(web)) {
		if (!qjs_init(web))
			return;
	}

	before_hash = hash_dom_snapshot(web);
	run_javascript(web->qjs->ctx, script, 0);
	after_hash = hash_dom_snapshot(web);

	if (before_hash != 0 && after_hash != 0 && before_hash != after_hash) {
		if (web->active_html != NULL) {
			HTMLRepaintFromDOM((HTMLInfo)web->active_html);
		}
	}
}

static void free_node_listeners(JSContext *ctx, HtmlDom_t *node) {
	if (!node)
		return;
	JS_Listener *el = node->listeners;
	while (el) {
		JS_Listener *tmp = el->next;
		free(el->type);
		free(el->id);
		if (ctx && !JS_IsUndefined(el->callback))
			JS_FreeValue(ctx, el->callback);
		free(el);
		el = tmp;
	}
	node->listeners = NULL;
	if (ctx && !JS_IsUndefined(node->js_onclick)) {
		JS_FreeValue(ctx, node->js_onclick);
		node->js_onclick = JS_UNDEFINED;
	}
	if (ctx && !JS_IsUndefined(node->js_object)) {
		JS_SetOpaque(node->js_object, NULL);
		JS_FreeValue(ctx, node->js_object);
		node->js_object = JS_UNDEFINED;
	}
}

static void js_free_dom_recursive(JSContext *ctx, HtmlDom_t *node) {
	if (!node)
		return;
	HtmlDom_t *c = node->first_child;
	while (c) {
		HtmlDom_t *next = c->next_sibling;
		js_free_dom_recursive(ctx, c);
		c = next;
	}
	free_node_listeners(ctx, node);
}

static void js_free_dom(HtmlDom_t *node) {
	if (!node)
		return;
	js_free_dom_recursive(gl_ctx.ctx, node);
	dom_free(node);
}

void qjs_exit(void) {
	g_run_pipeline = false;
	usleep(50000); /* Allow async threads to exit gracefully */

	if (gl_ctx.task_queue) {
		pthread_mutex_lock(&gl_ctx.task_mutex);
		JS_Task *task = gl_ctx.task_queue;
		while (task) {
			JS_Task *next = task->next;
			free(task->url);
			if (gl_ctx.ctx) {
				JS_FreeValue(gl_ctx.ctx, task->resolve);
				JS_FreeValue(gl_ctx.ctx, task->reject);
			}
			free(task);
			task = next;
		}
		gl_ctx.task_queue = NULL;
		pthread_mutex_unlock(&gl_ctx.task_mutex);
	}

	if (gl_ctx.observers) {
		JS_Observer *mo = gl_ctx.observers;
		while (mo) {
			JS_Observer *next = mo->next;
			if (gl_ctx.ctx)
				JS_FreeValue(gl_ctx.ctx, mo->callback);
			free(mo);
			mo = next;
		}
		gl_ctx.observers = NULL;
	}

	js_free_dom(gl_ctx.dom_root);
	gl_ctx.dom_root = NULL;

	if (gl_ctx.canvas) {
		free(gl_ctx.canvas->path_points);
		free(gl_ctx.canvas->segments);
		js_free(gl_ctx.ctx, gl_ctx.canvas);
		gl_ctx.canvas = NULL;
	}

	if (gl_ctx.js_buf) {
		free(gl_ctx.js_buf);
		gl_ctx.js_buf = NULL;
		gl_ctx.js_buf_size = 0;
	}

	if (gl_ctx.js_script) {
		free(gl_ctx.js_script);
		gl_ctx.js_script = NULL;
		gl_ctx.js_script_size = 0;
	}

	if (gl_ctx.ctx) {
		qjs_free_timers(gl_ctx.ctx);
		JS_FreeValue(gl_ctx.ctx, gl_ctx.js_obs);
		JS_FreeValue(gl_ctx.ctx, gl_ctx.js_this);
		js_std_free_handlers(gl_ctx.rt);
		JS_FreeContext(gl_ctx.ctx);
		JS_RunGC(gl_ctx.rt);
		JS_FreeRuntime(gl_ctx.rt);
		gl_ctx.ctx = NULL;
		gl_ctx.rt = NULL;
	}
}

static void notify_mutation(const char *type, HtmlDom_t *target) {
	if (!gl_ctx.ctx)
		return;

	JS_Observer *curr = gl_ctx.observers;
	while (curr) {
		if (!JS_IsUndefined(curr->callback) && !JS_IsNull(curr->callback) &&
		    (curr->target == NULL || curr->target == target)) {
			JSValue records = JS_NewArray(gl_ctx.ctx);
			JSValue record = JS_NewObject(gl_ctx.ctx);
			JS_SetPropertyStr(gl_ctx.ctx, record, "type",
				JS_NewString(gl_ctx.ctx, type));
			if (target) {
				JS_SetPropertyStr(gl_ctx.ctx, record, "target",
					create_js_dom_wrapper(gl_ctx.ctx, target));
			}
			JS_SetPropertyUint32(gl_ctx.ctx, records, 0, record);
			JSValue ret = JS_Call(gl_ctx.ctx, curr->callback, JS_UNDEFINED, 1, &records);
			JS_FreeValue(gl_ctx.ctx, ret);
			JS_FreeValue(gl_ctx.ctx, records);
		}
		curr = curr->next;
	}
}

static char *qjs_readfile(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return NULL;
	}
	fseek(f, 0, SEEK_SET);
	char *buf = malloc((size_t)sz + 1);
	if (buf) {
		size_t nread = fread(buf, 1, (size_t)sz, f);
		buf[nread] = '\0';
	}

	fclose(f);
	return buf;
}

static char *qjs_fetchfile(const char *url) {
	char host[256] = {0};
	char path[512] = {0};
	int port = 80;

	if (sscanf(url, "http://%255[^:/]:%d%511s", host, &port, path) < 2) {
		port = 80;
		if (sscanf(url, "http://%255[^/]%511s", host, path) < 1) {
			strncpy(host, url, sizeof(host) - 1);
			strcpy(path, "/");
		}
	}

	if (strlen(path) == 0)
		strcpy(path, "/");

	struct hostent *server = gethostbyname(host);
	if (!server)
		return NULL;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return NULL;

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons((uint16_t)port)
	};

	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, (size_t)server->h_length);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		return NULL;
	}

	char req[1024] = {0};
	snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
		path, host);
	send(sockfd, req, strlen(req), 0);

	size_t cap = 8192, total = 0;
	ssize_t n;
	char *resp = malloc(cap);
	if (!resp) {
		close(sockfd);
		return NULL;
	}

	while ((n = recv(sockfd, resp + total, cap - total - 1, 0)) > 0) {
		total += (size_t)n;
		if (total >= cap - 1) {
			cap *= 2;
			char *tmp = realloc(resp, cap);
			if (!tmp) {
				free(resp);
				close(sockfd);
				return NULL;
			}
			resp = tmp;
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
	(void)arg;
	while (g_run_pipeline) {
		JS_Task *task = NULL;
		pthread_mutex_lock(&gl_ctx.task_mutex);
		if (gl_ctx.task_queue) {
			task = gl_ctx.task_queue;
			gl_ctx.task_queue = gl_ctx.task_queue->next;
		}
		pthread_mutex_unlock(&gl_ctx.task_mutex);

		if (task) {
			JSContext *ctx = gl_ctx.ctx;
			if (!ctx) {
				free(task->url);
				free(task);
				continue;
			}

			if (task->url) {
				char *payload = qjs_fetchfile(task->url);
				if (payload && task->type == TASK_FETCH) {
					JSValue res_obj = JS_NewObject(ctx);
					JS_SetPropertyStr(ctx, res_obj, "text", JS_NewString(ctx, payload));
					JSValue argv = res_obj;
					JSValue ret = JS_Call(ctx, task->resolve, JS_UNDEFINED, 1, &argv);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, res_obj);
					JS_FreeValue(ctx, task->resolve);
					JS_FreeValue(ctx, task->reject);
					free(payload);
				} else if (payload && task->type == TASK_MODULE_IMPORT) {
					JSValue mod = JS_Eval(ctx, payload, strlen(payload), task->url,
						JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
					JSValue argv = mod;
					JSValue ret = JS_Call(ctx, task->resolve, JS_UNDEFINED, 1, &argv);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, mod);
					JS_FreeValue(ctx, task->resolve);
					JS_FreeValue(ctx, task->reject);
					free(payload);
				} else {
					JSValue err = JS_NewError(ctx);
					JSValue ret = JS_Call(ctx, task->reject, JS_UNDEFINED, 1, &err);
					JS_FreeValue(ctx, ret);
					JS_FreeValue(ctx, err);
					JS_FreeValue(ctx, task->resolve);
					JS_FreeValue(ctx, task->reject);
				}
				free(task->url);
				free(task);
			}

			JSContext *pctx;
			while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &pctx) > 0);
		} else {
			usleep(1000);
		}
	}

	return NULL;
}

static JSValue js_global_fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc < 1)
		return JS_EXCEPTION;

	const char *url = JS_ToCString(ctx, argv[0]);
	if (!url)
		return JS_EXCEPTION;

	JSValue resolving_funcs[2];
	JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
	if (JS_IsException(promise)) {
		JS_FreeCString(ctx, url);
		return JS_EXCEPTION;
	}

	JS_Task *task = calloc(1, sizeof(JS_Task));
	task->type = TASK_FETCH;
	task->url = strdup(url);
	task->resolve = JS_DupValue(ctx, resolving_funcs[0]);
	task->reject = JS_DupValue(ctx, resolving_funcs[1]);

	JS_FreeValue(ctx, resolving_funcs[0]);
	JS_FreeValue(ctx, resolving_funcs[1]);

	pthread_mutex_lock(&gl_ctx.task_mutex);
	task->next = gl_ctx.task_queue;
	gl_ctx.task_queue = task;
	pthread_mutex_unlock(&gl_ctx.task_mutex);
	JS_FreeCString(ctx, url);
	return promise;
}

static JSModuleDef *js_async_module_loader(JSContext *ctx, const char *name, void *opaque) {
	(void)opaque;
	if (name && (strncmp(name, "http://", 7) == 0
		|| strncmp(name, "https://", 8) == 0)) {
		JSValue resolving_funcs[2];
		JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

		JS_Task *task = calloc(1, sizeof(JS_Task));
		task->type = TASK_MODULE_IMPORT;
		task->url = strdup(name);
		task->resolve = JS_DupValue(ctx, resolving_funcs[0]);
		task->reject = JS_DupValue(ctx, resolving_funcs[1]);

		JS_FreeValue(ctx, resolving_funcs[0]);
		JS_FreeValue(ctx, resolving_funcs[1]);

		pthread_mutex_lock(&gl_ctx.task_mutex);
		task->next = gl_ctx.task_queue;
		gl_ctx.task_queue = task;
		pthread_mutex_unlock(&gl_ctx.task_mutex);

		JSModuleDef *m = JS_NewCModule(ctx, name, NULL);
		JS_FreeValue(ctx, promise);
		return m;
	}

	return JS_NewCModule(ctx, name, NULL);
}

static void run_javascript(JSContext *ctx, const char *script, int is_mod) {
	if (!ctx || !script)
		return;

	JSValue v = JS_Eval(ctx, script, strlen(script), "<script>",
		(is_mod ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL));
	js_std_loop(ctx);
	if (JS_IsException(v)) {
		JSValue stack = JS_GetPropertyStr(ctx, v, "stack");
		const char *s = JS_ToCString(ctx, stack);
		JSValue ex = JS_GetException(ctx);
		const char *msg = JS_ToCString(ctx, ex);
		fprintf(stderr, "Exception: %s\n%s\n", msg ? msg : "", s ? s : "");
		if (s)
			JS_FreeCString(ctx, s);
		if (msg)
			JS_FreeCString(ctx, msg);
		JS_FreeValue(ctx, stack);
		JS_FreeValue(ctx, ex);
	}

	JS_FreeValue(ctx, v);
}

static HtmlDom_t *find_clicked_node(HtmlDom_t *node, int mx, int my) {
	if (!node)
		return NULL;

	/* Hit-test children in reverse order (topmost z-order first) */
	HtmlDom_t *c = node->first_child;
	HtmlDom_t *hit = NULL;
	while (c) {
		HtmlDom_t *child_hit = find_clicked_node(c, mx, my);
		if (child_hit)
			hit = child_hit;
		c = c->next_sibling;
	}
	if (hit)
		return hit;

	int ay = node->y - gl_ctx.scroll_offset_y;
	if (node->type == NODE_ELEMENT &&
	    mx >= node->x && mx <= node->x + node->width &&
	    my >= ay && my <= ay + node->height) {
		return node;
	}

	return NULL;
}

static void dispatch_dom_event_internal(JSContext *ctx, HtmlDom_t *target, const char *type, JSValueConst event_obj) {
	if (!ctx || !target || !type)
		return;

	/* Bubble up the DOM tree */
	HtmlDom_t *curr_node = target;
	while (curr_node) {
		JSValue this_elem = create_js_dom_wrapper(ctx, curr_node);
		JS_Listener *curr = curr_node->listeners;
		while (curr) {
			JS_Listener *next = curr->next;
			if (curr->type && strcmp(curr->type, type) == 0) {
				JSValue arg = event_obj;
				JSValue ret = JS_Call(ctx, curr->callback, this_elem, 1, &arg);
				if (JS_IsException(ret)) {
					JSValue ex = JS_GetException(ctx);
					JS_FreeValue(ctx, ex);
				}
				JS_FreeValue(ctx, ret);
			}
			curr = next;
		}

		if (strcmp(type, "click") == 0) {
			if (!JS_IsUndefined(curr_node->js_onclick) && !JS_IsNull(curr_node->js_onclick)) {
				JSValue arg = event_obj;
				JSValue ret = JS_Call(ctx, curr_node->js_onclick, this_elem, 1, &arg);
				if (JS_IsException(ret)) {
					JSValue ex = JS_GetException(ctx);
					JS_FreeValue(ctx, ex);
				}
				JS_FreeValue(ctx, ret);
			} else {
				const char *attr_code = dom_get_attr(curr_node, "onclick");
				if (attr_code && *attr_code) {
					JSValue fn = JS_Eval(ctx, attr_code, strlen(attr_code), "<onclick>", JS_EVAL_TYPE_GLOBAL);
					if (JS_IsException(fn)) {
						JSValue ex = JS_GetException(ctx);
						JS_FreeValue(ctx, ex);
					}
					JS_FreeValue(ctx, fn);
				}
			}
		}

		JS_FreeValue(ctx, this_elem);
		curr_node = curr_node->parent;
	}
}

static JSValue js_event_prevent_default(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)ctx; (void)this_val; (void)argc; (void)argv;
	return JS_UNDEFINED;
}

static void dispatch_mouse_event(JSContext *ctx, HtmlDom_t *target, const char *type, int mx, int my, int button) {
	if (!ctx || !target || !type)
		return;

	JSValue ev_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, ev_obj, "type", JS_NewString(ctx, type));
	JS_SetPropertyStr(ctx, ev_obj, "clientX", JS_NewInt32(ctx, mx));
	JS_SetPropertyStr(ctx, ev_obj, "clientY", JS_NewInt32(ctx, my));
	JS_SetPropertyStr(ctx, ev_obj, "which", JS_NewInt32(ctx, button));
	JS_SetPropertyStr(ctx, ev_obj, "button", JS_NewInt32(ctx, button > 0 ? button - 1 : 0));
	JS_SetPropertyStr(ctx, ev_obj, "preventDefault",
		JS_NewCFunction(ctx, js_event_prevent_default, "preventDefault", 0));
	JS_SetPropertyStr(ctx, ev_obj, "target", create_js_dom_wrapper(ctx, target));

	/* Invoke on<type> handler property (e.g., canvas.onmousedown) on target */
	char prop_name[32];
	snprintf(prop_name, sizeof(prop_name), "on%s", type);
	JSValue target_js = create_js_dom_wrapper(ctx, target);
	JSValue handler = JS_GetPropertyStr(ctx, target_js, prop_name);
	if (JS_IsFunction(ctx, handler)) {
		JSValue ret = JS_Call(ctx, handler, target_js, 1, &ev_obj);
		if (JS_IsException(ret)) {
			JSValue ex = JS_GetException(ctx);
			JS_FreeValue(ctx, ex);
		}
		JS_FreeValue(ctx, ret);
	}
	JS_FreeValue(ctx, handler);
	JS_FreeValue(ctx, target_js);

	dispatch_dom_event_internal(ctx, target, type, ev_obj);
	JS_FreeValue(ctx, ev_obj);
}

void qjs_mouse_event(Widget w, XtPointer data, XEvent *ev, Boolean *cont) {
	(void)w;
	(void)data;
	(void)cont;
	if (ev->type == ButtonPress) {
		int mx = ev->xbutton.x, my = ev->xbutton.y;
		HtmlDom_t *target = find_clicked_node(gl_ctx.dom_root, mx, my);
		if (target && gl_ctx.ctx) {
			dispatch_mouse_event(gl_ctx.ctx, target, "mousedown", mx, my, ev->xbutton.button);
			if (ev->xbutton.button == Button3)
				dispatch_mouse_event(gl_ctx.ctx, target, "contextmenu", mx, my, ev->xbutton.button);
		}
	} else if (ev->type == ButtonRelease) {
		int mx = ev->xbutton.x, my = ev->xbutton.y;
		HtmlDom_t *target = find_clicked_node(gl_ctx.dom_root, mx, my);
		if (target && gl_ctx.ctx) {
			dispatch_mouse_event(gl_ctx.ctx, target, "mouseup", mx, my, ev->xbutton.button);
			if (ev->xbutton.button == Button1)
				dispatch_mouse_event(gl_ctx.ctx, target, "click", mx, my, ev->xbutton.button);
		}
	} else if (ev->type == MotionNotify) {
		int mx = ev->xbutton.x, my = ev->xbutton.y;
		HtmlDom_t *target = find_clicked_node(gl_ctx.dom_root, mx, my);
		if (target && gl_ctx.ctx) {
			dispatch_mouse_event(gl_ctx.ctx, target, "mousemove", mx, my, 0);
		}
	}
}

struct QJSTimerData_S {
	bow_t         web;
	JSValue       callback;
	XtIntervalId  tid;
	bool          active;
	QJSTimerData *prev;
	QJSTimerData *next;
};

static void qjs_unlink_timer(QJSTimerData *td) {
	if (!td)
		return;
	if (td->prev)
		td->prev->next = td->next;
	else if (gl_ctx.timers == td)
		gl_ctx.timers = td->next;
	if (td->next)
		td->next->prev = td->prev;
	td->prev = NULL;
	td->next = NULL;
}

static void qjs_free_timers(JSContext *ctx) {
	QJSTimerData *curr = gl_ctx.timers;
	gl_ctx.timers = NULL;
	while (curr) {
		QJSTimerData *next = curr->next;
		if (curr->active && curr->tid) {
			XtRemoveTimeOut(curr->tid);
			curr->active = false;
		}
		if (ctx && !JS_IsUndefined(curr->callback)) {
			JS_FreeValue(ctx, curr->callback);
			curr->callback = JS_UNDEFINED;
		}
		free(curr);
		curr = next;
	}
}

static void qjs_xt_timeout_cb(XtPointer client_data, XtIntervalId *id) {
	QJSTimerData *td = (QJSTimerData *)client_data;
	JSContext *ctx;
	JSContext *pctx;

	(void)id;
	if (!td)
		return;

	td->active = false;
	qjs_unlink_timer(td);

	if (td->web && td->web->qjs && td->web->qjs->ctx) {
		ctx = td->web->qjs->ctx;
		JSValue cb = td->callback;
		td->callback = JS_UNDEFINED;
		free(td);

		if (!JS_IsUndefined(cb) && !JS_IsNull(cb)) {
			JSValue ret = JS_Call(ctx, cb, JS_UNDEFINED, 0, NULL);
			if (JS_IsException(ret)) {
				JSValue ex = JS_GetException(ctx);
				const char *msg = JS_ToCString(ctx, ex);
				fprintf(stderr, "Timer Exception: %s\n", msg ? msg : "(unknown)");
				if (msg)
					JS_FreeCString(ctx, msg);
				JS_FreeValue(ctx, ex);
			}
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, cb);
		}
		while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &pctx) > 0)
			;
	} else {
		free(td);
	}
}

static JSValue js_global_set_timeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	bow_t web = (bow_t)JS_GetContextOpaque(ctx);
	int32_t delay = 0;
	QJSTimerData *td;

	(void)this_val;
	if (argc < 1 || !JS_IsFunction(ctx, argv[0]) || !web || !web->appcon)
		return JS_UNDEFINED;

	if (argc >= 2)
		JS_ToInt32(ctx, &delay, argv[1]);
	if (delay < 0)
		delay = 0;

	td = (QJSTimerData *)calloc(1, sizeof(QJSTimerData));
	if (!td)
		return JS_EXCEPTION;

	td->web = web;
	td->callback = JS_DupValue(ctx, argv[0]);
	td->active = true;
	td->next = gl_ctx.timers;
	if (gl_ctx.timers)
		gl_ctx.timers->prev = td;
	gl_ctx.timers = td;

	td->tid = XtAppAddTimeOut(web->appcon, (unsigned long)delay, qjs_xt_timeout_cb, td);
	return JS_NewInt64(ctx, (int64_t)td->tid);
}

void qjs_fire_onload(bow_t web) {
	JSContext *ctx;
	JSValue global, onload_fn, ret;
	JSContext *pctx;

	if (!web || !web->qjs || !web->qjs->ctx)
		return;

	ctx = web->qjs->ctx;
	global = JS_GetGlobalObject(ctx);
	onload_fn = JS_GetPropertyStr(ctx, global, "onload");

	if (JS_IsFunction(ctx, onload_fn)) {
		ret = JS_Call(ctx, onload_fn, global, 0, NULL);
		if (JS_IsException(ret)) {
			JSValue ex = JS_GetException(ctx);
			const char *msg = JS_ToCString(ctx, ex);
			JSValue stack = JS_GetPropertyStr(ctx, ex, "stack");
			const char *s = JS_ToCString(ctx, stack);
			fprintf(stderr, "window.onload Exception: %s\n%s\n", msg ? msg : "", s ? s : "");
			if (s)
				JS_FreeCString(ctx, s);

			if (msg)
				JS_FreeCString(ctx, msg);
			JS_FreeValue(ctx, stack);
			JS_FreeValue(ctx, ex);
		}
		JS_FreeValue(ctx, ret);
	}

	JS_FreeValue(ctx, onload_fn);
	JS_FreeValue(ctx, global);

	while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &pctx) > 0)
		;
}

#if defined(USE_QUICKJS_NG) || defined(QUICKJS_NG)
static FORCEINLINE void *my_malloc(void *opaque, size_t size) {
	(void)opaque;
	return malloc(size);
}

static FORCEINLINE void my_free(void *opaque, void *ptr) {
	(void)opaque;
	free(ptr);
}

static FORCEINLINE void *my_realloc(void *opaque, void *ptr, size_t size) {
	(void)opaque;
	return realloc(ptr, size);
}

static FORCEINLINE void *my_calloc(void *opaque, size_t count, size_t size) {
	(void)opaque;
	return calloc(count, size);
}

static FORCEINLINE size_t my_malloc_usable_size(const void *ptr) {
	return malloc_usable_size((void *)ptr);
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
	(void)opaque;
	return malloc(size);
}

static FORCEINLINE void my_free(JSMallocState *opaque, void *ptr) {
	(void)opaque;
	free(ptr);
}

static FORCEINLINE void *my_realloc(JSMallocState *opaque, void *ptr, size_t size) {
	(void)opaque;
	return realloc(ptr, size);
}

static FORCEINLINE size_t my_malloc_usable_size(const void *ptr) {
	return malloc_usable_size((void *)ptr);
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
}

static void js_object_finalizer(JSRuntime *rt, JSValue val) {
}

static void canvas_finalizer(JSRuntime *rt, JSValue val) {
}

static JSClassDef js_dom_obs_class = { "JS_Observer" };
static JSClassDef js_dom_node_class = {
	"HtmlDom_t",
	.finalizer = js_object_finalizer,
	.gc_mark = js_object_mark,
};
static JSClassDef js_dom_canvas_class = {
	"JS_Canvas",
	.finalizer = canvas_finalizer
};


static void push_log(bow_t web, const char *text) {
	if (!web || !web->qjs || !text)
		return;

	size_t text_len = strlen(text);
	size_t current_len = !web->qjs->js_buf ? 0 : strlen(web->qjs->js_buf);

	if (!web->qjs->js_buf || current_len + text_len + 1 >= web->qjs->js_buf_size) {
		size_t new_size = web->qjs->js_buf_size + text_len + 1024;
		char *nbuf = realloc(web->qjs->js_buf, new_size);
		if (!nbuf)
			return;
		if (!web->qjs->js_buf)
			nbuf[0] = '\0';
		web->qjs->js_buf = nbuf;
		web->qjs->js_buf_size = new_size;
	}

	strcat(web->qjs->js_buf, text);
}

/* pipe JS console output to buffer */
static JSValue qjs_console_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
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
	(void)ctx;
	if (argc < 1)
		return JS_EXCEPTION;

	JS_Observer *obs = JS_GetOpaque(this_val, gl_ctx.observer_class_id);
	HtmlDom_t *target = JS_GetOpaque(argv[0], gl_ctx.js_class_id);
	if (obs && target) {
		obs->target = target;
	}

	return JS_UNDEFINED;
}

static JSValue js_mutation_observer_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc < 1)
		return JS_EXCEPTION;

	JSValue js_obj = JS_NewObjectClass(ctx, gl_ctx.observer_class_id);
	JS_SetPrototype(ctx, js_obj, gl_ctx.js_obs);
	JS_Observer *obs = calloc(1, sizeof(JS_Observer));
	obs->callback = JS_DupValue(ctx, argv[0]);
	obs->target = NULL;
	obs->next = gl_ctx.observers;
	gl_ctx.observers = obs;
	JS_SetOpaque(js_obj, obs);
	return js_obj;
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

FORCEINLINE char *qjs_get_script(bow_t web, unsigned long *size) {
	*size = web->qjs->js_script_size;
	return web->qjs->js_script;
}

FORCEINLINE void qjs_set_script(bow_t web, char *data, unsigned long size) {
	if (web && web->qjs) {
		web->qjs->js_script_size = size;
		web->qjs->js_script = data;
	}
}

void qjs_append_script(bow_t web, const char *text, size_t text_len) {
	unsigned long js_buf_size = 0;
	char *js_buf = qjs_get_script(web, &js_buf_size);
	size_t current_len = !js_buf ? 0 : strlen(js_buf);

	if (current_len + text_len >= js_buf_size) {
		js_buf_size += text_len + (1024 * 1024);
		js_buf = realloc(js_buf, js_buf_size);
	}

	qjs_set_script(web, strcat(js_buf, text), js_buf_size);
}

FORCEINLINE void qjs_free_script(bow_t web) {
	if (web && web->qjs && web->qjs->js_script) {
		free(web->qjs->js_script);
		web->qjs->js_script_size = 0;
		web->qjs->js_script = NULL;
	}
}

static JSValue create_js_dom_wrapper(JSContext *ctx, HtmlDom_t *node) {
	if (!node)
		return JS_NULL;

	JSValue obj;
	if (node->tag_id == TAG_CANVAS) {
		obj = JS_NewObjectClass(ctx, gl_ctx.canvas_class_id);
		if (JS_IsException(obj))
			return obj;

		JS_SetOpaque(obj, node);
		define_accessor(ctx, obj, "width", (JSCFunction *)js_canvas_get_width, (JSCFunction *)js_canvas_set_width);
		define_accessor(ctx, obj, "height", (JSCFunction *)js_canvas_get_height, (JSCFunction *)js_canvas_set_height);
		JS_SetPropertyStr(ctx, obj, "getContext",
			JS_NewCFunction(ctx, js_canvas_get_context, "getContext", 1));
		JS_SetPropertyStr(ctx, obj, "getBoundingClientRect",
			JS_NewCFunction(ctx, js_getBoundingRect, "getBoundingClientRect", 0));
	} else {
		obj = JS_NewObjectClass(ctx, gl_ctx.js_class_id);
		if (JS_IsException(obj))
			return obj;
		JS_SetOpaque(obj, node);
	}

	return obj;
}

static JSValue create_js_nodelist(JSContext *ctx, HtmlDom_t **items, size_t count) {
	JSValue arr = JS_NewArray(ctx);
	size_t i;
	for (i = 0; i < count; i++) {
		JS_SetPropertyUint32(ctx, arr, (uint32_t)i, create_js_dom_wrapper(ctx, items[i]));
	}
	return arr;
}

/* ============================================================================
 * DOM Traversal & Query Helpers
 * ============================================================================ */

static HtmlDom_t *find_dom_by_id(HtmlDom_t *root, const char *id) {
	if (!root || !id)
		return NULL;

	if (root->type == NODE_ELEMENT) {
		const char *val = dom_get_attr(root, "id");
		if (val && strcmp(val, id) == 0)
			return root;
	}

	HtmlDom_t *c = root->first_child;
	while (c) {
		HtmlDom_t *res = find_dom_by_id(c, id);
		if (res)
			return res;
		c = c->next_sibling;
	}

	return NULL;
}

static void find_dom_by_class_rec(HtmlDom_t *node, const char *cls, HtmlDom_t ***items, size_t *count, size_t *cap) {
	if (!node || !cls)
		return;

	if (node->type == NODE_ELEMENT) {
		const char *val = dom_get_attr(node, "class");
		if (val) {
			size_t clen = strlen(cls);
			const char *p = val;
			while (*p) {
				while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
					p++;
				if (!*p)
					break;
				const char *start = p;
				while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
					p++;
				if ((size_t)(p - start) == clen && strncmp(start, cls, clen) == 0) {
					if (*count == *cap) {
						*cap = (*cap == 0) ? 16 : (*cap * 2);
						*items = realloc(*items, sizeof(HtmlDom_t *) * (*cap));
					}
					(*items)[(*count)++] = node;
					break;
				}
			}
		}
	}

	HtmlDom_t *c = node->first_child;
	while (c) {
		find_dom_by_class_rec(c, cls, items, count, cap);
		c = c->next_sibling;
	}
}

/* ============================================================================
 * DOM Element / Node Properties (Getters & Setters)
 * ============================================================================ */

static JSValue js_dom_get_tag_name(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	if (node->type == NODE_ELEMENT && node->tag_name) {
		char *upper = strdup(node->tag_name);
		size_t i;
		for (i = 0; upper[i]; i++)
			upper[i] = (char)toupper((unsigned char)upper[i]);
		JSValue res = JS_NewString(ctx, upper);
		free(upper);
		return res;
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_node_name(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	if (node->type == NODE_ELEMENT)
		return js_dom_get_tag_name(ctx, this_val, argc, argv);
	if (node->type == NODE_TEXT)
		return JS_NewString(ctx, "#text");
	if (node->type == NODE_COMMENT)
		return JS_NewString(ctx, "#comment");
	if (node->type == NODE_DOCUMENT)
		return JS_NewString(ctx, "#document");
	return JS_UNDEFINED;
}

static JSValue js_dom_get_node_type(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	/* Standard W3C DOM nodeType constants */
	switch (node->type) {
		case NODE_ELEMENT:  return JS_NewInt32(ctx, 1); /* ELEMENT_NODE */
		case NODE_TEXT:     return JS_NewInt32(ctx, 3); /* TEXT_NODE */
		case NODE_COMMENT:  return JS_NewInt32(ctx, 8); /* COMMENT_NODE */
		case NODE_DOCUMENT: return JS_NewInt32(ctx, 9); /* DOCUMENT_NODE */
		default:            return JS_NewInt32(ctx, 0);
	}
}

static JSValue js_dom_get_id(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_NewString(ctx, "");

	const char *val = dom_get_attr(node, "id");
	return JS_NewString(ctx, val ? val : "");
}

static JSValue js_dom_set_id(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_UNDEFINED;

	const char *str = JS_ToCString(ctx, argv[0]);
	if (str) {
		dom_set_attr(node, "id", str);
		JS_FreeCString(ctx, str);
		notify_mutation("attributes", node);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_class_name(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_NewString(ctx, "");

	const char *val = dom_get_attr(node, "class");
	return JS_NewString(ctx, val ? val : "");
}

static JSValue js_dom_set_class_name(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_UNDEFINED;

	const char *str = JS_ToCString(ctx, argv[0]);
	if (str) {
		dom_set_attr(node, "class", str);
		JS_FreeCString(ctx, str);
		notify_mutation("attributes", node);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_text_content(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;

	char *txt = dom_get_text_content(node);
	JSValue res = JS_NewString(ctx, txt ? txt : "");
	free(txt);
	return res;
}

static JSValue js_dom_set_text_content(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	const char *str = JS_ToCString(ctx, argv[0]);
	if (str) {
		dom_set_text_content(node, str);
		JS_FreeCString(ctx, str);
		notify_mutation("childList", node);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_inner_html(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NewString(ctx, "");

	char *html = dom_get_inner_html(node, false);
	JSValue res = JS_NewString(ctx, html ? html : "");
	free(html);
	return res;
}

static JSValue js_dom_set_inner_html(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	const char *str = JS_ToCString(ctx, argv[0]);
	if (str) {
		dom_set_inner_html(node, str);
		JS_FreeCString(ctx, str);
		notify_mutation("childList", node);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_outer_html(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NewString(ctx, "");

	char *html = dom_serialize(node, false);
	JSValue res = JS_NewString(ctx, html ? html : "");
	free(html);
	return res;
}

static JSValue js_dom_set_outer_html(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !node->parent)
		return JS_UNDEFINED;

	HtmlDom_t *parent = node->parent;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (str) {
		JS_SetOpaque((JSValue)this_val, NULL);
		dom_set_outer_html(node, str);
		JS_FreeCString(ctx, str);
		notify_mutation("childList", parent);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_get_parent_node(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !node->parent)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, node->parent);
}

static JSValue js_dom_get_first_child(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !node->first_child)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, node->first_child);
}

static JSValue js_dom_get_last_child(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !node->last_child)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, node->last_child);
}

static JSValue js_dom_get_next_sibling(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !node->next_sibling)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, node->next_sibling);
}

static JSValue js_dom_get_previous_sibling(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	HtmlDom_t *prev = dom_previous_sibling(node);
	if (!prev)
		return JS_NULL;
	return create_js_dom_wrapper(ctx, prev);
}

static JSValue js_dom_get_first_element_child(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	HtmlDom_t *c = node->first_child;
	while (c) {
		if (c->type == NODE_ELEMENT)
			return create_js_dom_wrapper(ctx, c);
		c = c->next_sibling;
	}
	return JS_NULL;
}

static JSValue js_dom_get_last_element_child(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	HtmlDom_t *last_elem = NULL;
	HtmlDom_t *c = node->first_child;
	while (c) {
		if (c->type == NODE_ELEMENT)
			last_elem = c;
		c = c->next_sibling;
	}
	return last_elem ? create_js_dom_wrapper(ctx, last_elem) : JS_NULL;
}

static JSValue js_dom_get_next_element_sibling(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	HtmlDom_t *s = node->next_sibling;
	while (s) {
		if (s->type == NODE_ELEMENT)
			return create_js_dom_wrapper(ctx, s);
		s = s->next_sibling;
	}
	return JS_NULL;
}

static JSValue js_dom_get_previous_element_sibling(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	HtmlDom_t *s = dom_previous_sibling(node);
	while (s) {
		if (s->type == NODE_ELEMENT)
			return create_js_dom_wrapper(ctx, s);
		s = dom_previous_sibling(s);
	}
	return JS_NULL;
}

static JSValue js_dom_get_child_nodes(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	JSValue arr = JS_NewArray(ctx);
	if (!node)
		return arr;

	uint32_t idx = 0;
	HtmlDom_t *c = node->first_child;
	while (c) {
		JS_SetPropertyUint32(ctx, arr, idx++, create_js_dom_wrapper(ctx, c));
		c = c->next_sibling;
	}
	return arr;
}

static JSValue js_dom_get_children(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	JSValue arr = JS_NewArray(ctx);
	if (!node)
		return arr;

	uint32_t idx = 0;
	HtmlDom_t *c = node->first_child;
	while (c) {
		if (c->type == NODE_ELEMENT) {
			JS_SetPropertyUint32(ctx, arr, idx++, create_js_dom_wrapper(ctx, c));
		}
		c = c->next_sibling;
	}
	return arr;
}

static JSValue js_dom_get_length(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NewInt32(ctx, 0);

	if (node->type == NODE_TEXT || node->type == NODE_COMMENT) {
		return JS_NewInt32(ctx, node->text ? (int32_t)strlen(node->text) : 0);
	}

	int32_t count = 0;
	HtmlDom_t *c = node->first_child;
	while (c) {
		count++;
		c = c->next_sibling;
	}
	return JS_NewInt32(ctx, count);
}

static JSValue js_dom_get_child_element_count(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NewInt32(ctx, 0);

	int32_t count = 0;
	HtmlDom_t *c = node->first_child;
	while (c) {
		if (c->type == NODE_ELEMENT)
			count++;
		c = c->next_sibling;
	}
	return JS_NewInt32(ctx, count);
}

static JSValue js_dom_get_onclick(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;
	return JS_IsUndefined(node->js_onclick) ? JS_NULL : JS_DupValue(ctx, node->js_onclick);
}

static JSValue js_dom_set_onclick(JSContext *ctx, JSValueConst this_val, int argc, JSValue *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;
	if (!JS_IsUndefined(node->js_onclick))
		JS_FreeValue(ctx, node->js_onclick);
	node->js_onclick = JS_IsFunction(ctx, argv[0]) ? JS_DupValue(ctx, argv[0]) : JS_UNDEFINED;
	return JS_UNDEFINED;
}

/* ============================================================================
 * DOM Mutation & Manipulation Methods
 * ============================================================================ */

static JSValue js_dom_append_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *parent = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	HtmlDom_t *child = JS_GetOpaque(argv[0], gl_ctx.js_class_id);
	if (!parent || !child)
		return JS_ThrowTypeError(ctx, "appendChild requires valid DOM nodes");

	dom_append_child(parent, child);
	notify_mutation("childList", parent);
	return JS_DupValue(ctx, argv[0]);
}

static JSValue js_dom_insert_before(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	HtmlDom_t *parent = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	HtmlDom_t *new_node = JS_GetOpaque(argv[0], gl_ctx.js_class_id);
	HtmlDom_t *ref_node = JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])
		? NULL : JS_GetOpaque(argv[1], gl_ctx.js_class_id);

	if (!parent || !new_node)
		return JS_ThrowTypeError(ctx, "insertBefore requires valid DOM nodes");

	dom_insert_before(parent, new_node, ref_node);
	notify_mutation("childList", parent);
	return JS_DupValue(ctx, argv[0]);
}

static JSValue js_dom_remove_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *parent = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	HtmlDom_t *child = JS_GetOpaque(argv[0], gl_ctx.js_class_id);
	if (!parent || !child)
		return JS_ThrowTypeError(ctx, "removeChild requires valid DOM nodes");

	HtmlDom_t *removed = dom_remove_child(parent, child);
	if (!removed)
		return JS_ThrowRangeError(ctx, "Node to be removed is not a child of this node");

	notify_mutation("childList", parent);
	return JS_DupValue(ctx, argv[0]);
}

static JSValue js_dom_replace_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	HtmlDom_t *parent = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	HtmlDom_t *new_child = JS_GetOpaque(argv[0], gl_ctx.js_class_id);
	HtmlDom_t *old_child = JS_GetOpaque(argv[1], gl_ctx.js_class_id);
	if (!parent || !new_child || !old_child)
		return JS_ThrowTypeError(ctx, "replaceChild requires valid DOM nodes");

	HtmlDom_t *replaced = dom_replace_child(parent, new_child, old_child);
	if (!replaced)
		return JS_ThrowRangeError(ctx, "Failed to replace child node");

	notify_mutation("childList", parent);
	return JS_DupValue(ctx, argv[1]);
}

static JSValue js_dom_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)ctx;
	(void)argc;
	(void)argv;
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (node && node->parent) {
		HtmlDom_t *parent = node->parent;
		dom_remove_child(parent, node);
		notify_mutation("childList", parent);
	}
	return JS_UNDEFINED;
}

static JSValue js_dom_clone_node(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_NULL;

	bool deep = (argc >= 1) ? JS_ToBool(ctx, argv[0]) : false;
	HtmlDom_t *cloned = dom_clone_node(node, deep);
	return create_js_dom_wrapper(ctx, cloned);
}

static JSValue js_dom_has_child_nodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)ctx;
	(void)argc;
	(void)argv;
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	return JS_NewBool(ctx, node && node->first_child != NULL);
}

/* ============================================================================
 * Attribute Methods
 * ============================================================================ */

static JSValue js_dom_set_attribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_UNDEFINED;

	const char *name = JS_ToCString(ctx, argv[0]);
	const char *val = JS_ToCString(ctx, argv[1]);
	if (name && val) {
		dom_set_attr(node, name, val);
		notify_mutation("attributes", node);
	}
	if (name)
		JS_FreeCString(ctx, name);
	if (val)
		JS_FreeCString(ctx, val);
	return JS_UNDEFINED;
}

static JSValue js_dom_get_attribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_NULL;

	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_NULL;

	const char *val = dom_get_attr(node, name);
	JS_FreeCString(ctx, name);
	return val ? JS_NewString(ctx, val) : JS_NULL;
}

static JSValue js_dom_has_attribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_FALSE;

	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_FALSE;

	bool has = dom_has_attr(node, name);
	JS_FreeCString(ctx, name);
	return JS_NewBool(ctx, has);
}

static JSValue js_dom_remove_attribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || node->type != NODE_ELEMENT)
		return JS_UNDEFINED;

	const char *name = JS_ToCString(ctx, argv[0]);
	if (name) {
		if (dom_remove_attr(node, name)) {
			notify_mutation("attributes", node);
		}
		JS_FreeCString(ctx, name);
	}
	return JS_UNDEFINED;
}

/* ============================================================================
 * Selector & Element Query Methods (Element & Document)
 * ============================================================================ */

static JSValue js_dom_query_selector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *root = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!root)
		root = gl_ctx.dom_root;

	const char *sel = JS_ToCString(ctx, argv[0]);
	if (!sel)
		return JS_NULL;

	HtmlDom_t *found = dom_query_selector(root, sel);
	JS_FreeCString(ctx, sel);
	return found ? create_js_dom_wrapper(ctx, found) : JS_NULL;
}

static JSValue js_dom_query_selector_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *root = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!root)
		root = gl_ctx.dom_root;

	const char *sel = JS_ToCString(ctx, argv[0]);
	if (!sel)
		return JS_NewArray(ctx);

	HtmlNodeList *list = dom_query_selector_all(root, sel);
	JS_FreeCString(ctx, sel);
	if (!list)
		return JS_NewArray(ctx);

	JSValue arr = create_js_nodelist(ctx, list->items, list->count);
	dom_node_list_free(list);
	return arr;
}

static JSValue js_dom_get_elements_by_tag_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *root = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!root)
		root = gl_ctx.dom_root;

	const char *tag = JS_ToCString(ctx, argv[0]);
	if (!tag)
		return JS_NewArray(ctx);

	HtmlNodeList *list = dom_get_elements_by_tag_name(root, tag);
	JS_FreeCString(ctx, tag);
	if (!list)
		return JS_NewArray(ctx);

	JSValue arr = create_js_nodelist(ctx, list->items, list->count);
	dom_node_list_free(list);
	return arr;
}

static JSValue js_dom_get_elements_by_class_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_EXCEPTION;

	HtmlDom_t *root = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!root)
		root = gl_ctx.dom_root;

	const char *cls = JS_ToCString(ctx, argv[0]);
	if (!cls)
		return JS_NewArray(ctx);

	HtmlDom_t **items = NULL;
	size_t count = 0, cap = 0;
	find_dom_by_class_rec(root, cls, &items, &count, &cap);
	JS_FreeCString(ctx, cls);

	JSValue arr = create_js_nodelist(ctx, items, count);
	free(items);
	return arr;
}

static JSValue js_dom_get_element_by_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc < 1)
		return JS_EXCEPTION;

	const char *id = JS_ToCString(ctx, argv[0]);
	if (!id)
		return JS_NULL;

	HtmlDom_t *found = find_dom_by_id(gl_ctx.dom_root, id);
	JS_FreeCString(ctx, id);
	if (!found)
		return JS_NULL;

	return create_js_dom_wrapper(ctx, found);
}

static void ensure_path_capacity(JS_Canvas *cctx, int needed) {
	if (cctx->path_capacity < needed) {
		cctx->path_capacity = needed * 2;
		cctx->path_points = realloc(cctx->path_points,
			sizeof(XPoint) * cctx->path_capacity);
	}
}

static void ensure_seg_capacity(JS_Canvas *cctx, int needed) {
	if (cctx->seg_capacity < needed) {
		cctx->seg_capacity = needed * 2 + 64;
		cctx->segments = realloc(cctx->segments, sizeof(XSegment) * (size_t)cctx->seg_capacity);
	}
}

static JSValue js_beginPath(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val; (void)argc; (void)argv;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	cctx->path_count = 0;
	cctx->seg_count = 0;
	cctx->has_pen = false;
	return JS_UNDEFINED;
}

static JSValue js_moveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	double dx, dy;
	if (argc < 2 || JS_ToFloat64(ctx, &dx, argv[0]) || JS_ToFloat64(ctx, &dy, argv[1]))
		return JS_ThrowTypeError(ctx, "moveTo expects 2 numbers");

	int x = (int)dx;
	int y = (int)dy;
	cctx->pen_x = x;
	cctx->pen_y = y;
	cctx->has_pen = true;

	ensure_path_capacity(cctx, cctx->path_count + 1);
	cctx->path_points[cctx->path_count].x = (short)x;
	cctx->path_points[cctx->path_count].y = (short)y;
	cctx->path_count++;
	return JS_UNDEFINED;
}

static JSValue js_lineTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	double dx, dy;
	if (argc < 2 || JS_ToFloat64(ctx, &dx, argv[0]) || JS_ToFloat64(ctx, &dy, argv[1]))
		return JS_ThrowTypeError(ctx, "lineTo expects 2 numbers");

	int x = (int)dx;
	int y = (int)dy;
	if (!cctx->has_pen) {
		cctx->pen_x = x;
		cctx->pen_y = y;
		cctx->has_pen = true;
	} else {
		ensure_seg_capacity(cctx, cctx->seg_count + 1);
		cctx->segments[cctx->seg_count].x1 = (short)cctx->pen_x;
		cctx->segments[cctx->seg_count].y1 = (short)cctx->pen_y;
		cctx->segments[cctx->seg_count].x2 = (short)x;
		cctx->segments[cctx->seg_count].y2 = (short)y;
		cctx->seg_count++;
		cctx->pen_x = x;
		cctx->pen_y = y;
	}

	ensure_path_capacity(cctx, cctx->path_count + 1);
	cctx->path_points[cctx->path_count].x = (short)x;
	cctx->path_points[cctx->path_count].y = (short)y;
	cctx->path_count++;
	return JS_UNDEFINED;
}

static JSValue js_stroke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val; (void)argc; (void)argv;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	if (cctx->seg_count > 0) {
		const char *color = cctx->stroke_style[0] ? cctx->stroke_style : "#000000";
		HTMLCanvasStrokeSegments(cctx->segments, cctx->seg_count, color);
		HTMLCanvasFlush();
	}
	return JS_UNDEFINED;
}

static JSValue js_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val; (void)argc; (void)argv;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	if (cctx->path_count > 2) {
		const char *color = cctx->fill_style[0] ? cctx->fill_style : "#000000";
		HTMLCanvasFillPolygon(cctx->path_points, cctx->path_count, color);
		HTMLCanvasFlush();
	}
	return JS_UNDEFINED;
}

static JSValue js_getBoundingRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val; (void)argc; (void)argv;
	int x, y, w, h;
	HTMLCanvasGetBounds(&x, &y, &w, &h);

	JSValue rect = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, rect, "left", JS_NewInt32(ctx, x));
	JS_SetPropertyStr(ctx, rect, "top", JS_NewInt32(ctx, y));
	JS_SetPropertyStr(ctx, rect, "right", JS_NewInt32(ctx, x + w));
	JS_SetPropertyStr(ctx, rect, "bottom", JS_NewInt32(ctx, y + h));
	JS_SetPropertyStr(ctx, rect, "width", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, rect, "height", JS_NewInt32(ctx, h));
	JS_SetPropertyStr(ctx, rect, "x", JS_NewInt32(ctx, x));
	JS_SetPropertyStr(ctx, rect, "y", JS_NewInt32(ctx, y));
	return rect;
}

static JSValue js_ctx_get_stroke_style(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	return JS_NewString(ctx, (cctx && cctx->stroke_style[0]) ? cctx->stroke_style : "#000000");
}

static JSValue js_ctx_set_stroke_style(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (cctx) {
		const char *s = JS_ToCString(ctx, val);
		if (s) {
			snprintf(cctx->stroke_style, sizeof(cctx->stroke_style), "%s", s);
			JS_FreeCString(ctx, s);
		}
	}
	return JS_UNDEFINED;
}

static JSValue js_ctx_get_fill_style(JSContext *ctx, JSValueConst this_val) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	return JS_NewString(ctx, (cctx && cctx->fill_style[0]) ? cctx->fill_style : "#000000");
}

static JSValue js_ctx_set_fill_style(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
	(void)this_val;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (cctx) {
		const char *s = JS_ToCString(ctx, val);
		if (s) {
			snprintf(cctx->fill_style, sizeof(cctx->fill_style), "%s", s);
			JS_FreeCString(ctx, s);
		}
	}
	return JS_UNDEFINED;
}

static JSValue js_canvas_get_width(JSContext *ctx, JSValueConst this_val) {
	(void)this_val;
	int x, y, w, h;
	HTMLCanvasGetBounds(&x, &y, &w, &h);
	return JS_NewInt32(ctx, w);
}

static JSValue js_canvas_set_width(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.canvas_class_id);
	int32_t w = 300;
	if (JS_ToInt32(ctx, &w, val) < 0)
		return JS_EXCEPTION;
	if (w <= 0)
		w = 300;

	if (node) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", w);
		dom_set_attr(node, "width", buf);
	}
	int x, y, cur_w, cur_h;
	HTMLCanvasGetBounds(&x, &y, &cur_w, &cur_h);
	HTMLCanvasResize(w, cur_h);
	return JS_UNDEFINED;
}

static JSValue js_canvas_get_height(JSContext *ctx, JSValueConst this_val) {
	(void)this_val;
	int x, y, w, h;
	HTMLCanvasGetBounds(&x, &y, &w, &h);
	return JS_NewInt32(ctx, h);
}

static JSValue js_canvas_set_height(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.canvas_class_id);
	int32_t h = 150;
	if (JS_ToInt32(ctx, &h, val) < 0)
		return JS_EXCEPTION;
	if (h <= 0)
		h = 150;

	if (node) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", h);
		dom_set_attr(node, "height", buf);
	}
	int x, y, cur_w, cur_h;
	HTMLCanvasGetBounds(&x, &y, &cur_w, &cur_h);
	HTMLCanvasResize(cur_w, h);
	return JS_UNDEFINED;
}

// fillRect JS binding
static JSValue js_fillRect(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	trace;
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	int x, y, w, h;
	if (argc == 1) {
		JS_SetPropertyStr(ctx, this_val, "width", JS_NewInt32(ctx, cctx->canvas_w));
		JS_SetPropertyStr(ctx, this_val, "height", JS_NewInt32(ctx, cctx->canvas_h));
		return JS_UNDEFINED;
	}

	if (argc < 4 ||
		JS_ToInt32(ctx, &x, argv[0]) ||
		JS_ToInt32(ctx, &y, argv[1]) ||
		JS_ToInt32(ctx, &w, argv[2]) ||
		JS_ToInt32(ctx, &h, argv[3])) {
		return JS_ThrowTypeError(ctx, "fillRect expects 4 integers");
	}

	XFillRectangle(cctx->display, cctx->buffer, cctx->gc, x, y, w, h);
	HTMLCanvasFlush();
	return JS_UNDEFINED;
}

// clearRect JS binding
static JSValue js_clearRect(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	int x, y, w, h;
	if (argc == 1) {
		JS_SetPropertyStr(ctx, this_val, "width", JS_NewInt32(ctx, cctx->canvas_w));
		JS_SetPropertyStr(ctx, this_val, "height", JS_NewInt32(ctx, cctx->canvas_h));
		return JS_UNDEFINED;
	}

	if (argc < 4 ||
		JS_ToInt32(ctx, &x, argv[0]) ||
		JS_ToInt32(ctx, &y, argv[1]) ||
		JS_ToInt32(ctx, &w, argv[2]) ||
		JS_ToInt32(ctx, &h, argv[3])) {
		return JS_ThrowTypeError(ctx, "fillRect expects 4 integers");
	}

	HTMLCanvasClearRect(x, y, w, h);
	return JS_UNDEFINED;
}

// strokeRect JS binding
static JSValue js_strokeRect(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	JS_Canvas *cctx = gl_ctx.canvas;
	if (!cctx)
		return JS_ThrowTypeError(ctx, "Invalid Canvas context");

	int x, y, w, h;
	if (argc < 4 ||
		JS_ToInt32(ctx, &x, argv[0]) ||
		JS_ToInt32(ctx, &y, argv[1]) ||
		JS_ToInt32(ctx, &w, argv[2]) ||
		JS_ToInt32(ctx, &h, argv[3])) {
		return JS_ThrowTypeError(ctx, "strokeRect expects 4 integers");
	}

	XDrawRectangle(cctx->display, cctx->buffer, cctx->gc, x, y, w, h);
	HTMLCanvasFlush();
	return JS_UNDEFINED;
}

static JSValue create_canvas_context(JSContext *ctx, JSValue obj) {
	define_accessor(ctx, obj, "strokeStyle", (JSCFunction *)js_ctx_get_stroke_style,
		(JSCFunction *)js_ctx_set_stroke_style);
	define_accessor(ctx, obj, "fillStyle", (JSCFunction *)js_ctx_get_fill_style,
		(JSCFunction *)js_ctx_set_fill_style);
	JS_SetPropertyStr(ctx, obj, "beginPath",
		JS_NewCFunction(ctx, js_beginPath, "beginPath", 0));
	JS_SetPropertyStr(ctx, obj, "moveTo",
		JS_NewCFunction(ctx, js_moveTo, "moveTo", 2));
	JS_SetPropertyStr(ctx, obj, "lineTo",
		JS_NewCFunction(ctx, js_lineTo, "lineTo", 2));
	JS_SetPropertyStr(ctx, obj, "stroke",
		JS_NewCFunction(ctx, js_stroke, "stroke", 0));
	JS_SetPropertyStr(ctx, obj, "fill",
		JS_NewCFunction(ctx, js_fill, "fill", 0));
	JS_SetPropertyStr(ctx, obj, "fillRect",
		JS_NewCFunction(ctx, js_fillRect, "fillRect", 4));
	JS_SetPropertyStr(ctx, obj, "clearRect",
		JS_NewCFunction(ctx, js_clearRect, "clearRect", 4));
	JS_SetPropertyStr(ctx, obj, "strokeRect",
		JS_NewCFunction(ctx, js_strokeRect, "strokeRect", 4));
	return obj;
}

static JSValue js_canvas_get_context(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)argc; (void)argv;
	JSValue ctx_obj = JS_NewObject(ctx);
	create_canvas_context(ctx, ctx_obj);
	JS_SetPropertyStr(ctx, ctx_obj, "canvas", JS_DupValue(ctx, this_val));
	return ctx_obj;
}

/* ============================================================================
 * Event Listeners & Event Dispatching
 * ============================================================================ */

static JSValue js_dom_add_eventlistener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node || !JS_IsFunction(ctx, argv[1]))
		return JS_UNDEFINED;

	const char *type = JS_ToCString(ctx, argv[0]);
	if (type) {
		const char *id_val = dom_get_attr(node, "id");
		JS_Listener *listener = calloc(1, sizeof(JS_Listener));
		listener->type = strdup(type);
		listener->id = id_val ? strdup(id_val) : NULL;
		listener->callback = JS_DupValue(ctx, argv[1]);
		listener->next = node->listeners;
		node->listeners = listener;

		JS_FreeCString(ctx, type);
	}

	return JS_UNDEFINED;
}

static JSValue js_dom_remove_eventlistener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_EXCEPTION;

	HtmlDom_t *node = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!node)
		return JS_UNDEFINED;

	const char *type = JS_ToCString(ctx, argv[0]);
	if (type) {
		JS_Listener *prev = NULL;
		JS_Listener *curr = node->listeners;
		while (curr) {
			if (curr->type && strcmp(curr->type, type) == 0 &&
			    JS_VALUE_GET_PTR(curr->callback) == JS_VALUE_GET_PTR(argv[1])) {
				if (prev)
					prev->next = curr->next;
				else
					node->listeners = curr->next;
				free(curr->type);
				free(curr->id);
				JS_FreeValue(ctx, curr->callback);
				free(curr);
				break;
			}
			prev = curr;
			curr = curr->next;
		}
		JS_FreeCString(ctx, type);
	}
	return JS_UNDEFINED;
}
static JSValue js_dom_dispatch_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	HtmlDom_t *n = JS_GetOpaque(this_val, gl_ctx.js_class_id);
	if (!n || argc < 1)
		return JS_FALSE;

	/* 1. Standard W3C: el.dispatchEvent(new Event('click')) */
	if (JS_IsObject(argv[0])) {
		JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
		const char *ev_type = JS_ToCString(ctx, type_val);
		if (ev_type) {
			JS_SetPropertyStr(ctx, (JSValue)argv[0], "target", JS_DupValue(ctx, this_val));
			dispatch_dom_event_internal(ctx, n, ev_type, argv[0]);
			JS_FreeCString(ctx, ev_type);
		}
		JS_FreeValue(ctx, type_val);
	}

	/* 2. String shorthand: el.dispatchEvent('click') OR el.dispatchEvent('id', 'click') */
	else if (JS_IsString(argv[0])) {
		JSValue type_arg = (argc >= 2 && JS_IsString(argv[1])) ? argv[1] : argv[0];
		const char *ev_type = JS_ToCString(ctx, type_arg);
		if (ev_type) {
			JSValue ev_obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, ev_obj, "type", JS_NewString(ctx, ev_type));
			JS_SetPropertyStr(ctx, ev_obj, "target", JS_DupValue(ctx, this_val));
			dispatch_dom_event_internal(ctx, n, ev_type, ev_obj);
			JS_FreeValue(ctx, ev_obj);
			JS_FreeCString(ctx, ev_type);
		}
	}

	return JS_TRUE;
}

/* ============================================================================
 * Document Factory Methods & Properties
 * ============================================================================ */

static JSValue js_dom_create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc < 1)
		return JS_EXCEPTION;

	const char *tag = JS_ToCString(ctx, argv[0]);
	if (!tag)
		return JS_EXCEPTION;

	HtmlDom_t *native = dom_create_element(tag);
	JS_FreeCString(ctx, tag);
	return create_js_dom_wrapper(ctx, native);
}

static JSValue js_dom_create_text_node(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	const char *text = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : NULL;
	HtmlDom_t *native = dom_create_text(text ? text : "");
	if (text)
		JS_FreeCString(ctx, text);
	return create_js_dom_wrapper(ctx, native);
}

static JSValue js_dom_create_comment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	const char *text = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : NULL;
	HtmlDom_t *native = dom_create_comment(text ? text : "");
	if (text)
		JS_FreeCString(ctx, text);
	return create_js_dom_wrapper(ctx, native);
}

static JSValue js_event_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc < 1)
		return JS_EXCEPTION;

	JSValue ev = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, ev, "type", JS_DupValue(ctx, argv[0]));
	JS_SetPropertyStr(ctx, ev, "bubbles", JS_TRUE);
	JS_SetPropertyStr(ctx, ev, "cancelable", JS_TRUE);
	if (argc >= 2 && JS_IsObject(argv[1])) {
		JSValue detail = JS_GetPropertyStr(ctx, argv[1], "detail");
		if (!JS_IsUndefined(detail))
			JS_SetPropertyStr(ctx, ev, "detail", detail);
		else
			JS_FreeValue(ctx, detail);
	}
	return ev;
}

/* ============================================================================
 * Prototype & Global Interface Registration
 * ============================================================================ */

static void define_accessor(JSContext *ctx, JSValue proto, const char *name,
                            JSCFunction *getter, JSCFunction *setter) {
	JSAtom atom = JS_NewAtom(ctx, name);
	JSValue get_fn = getter ? JS_NewCFunction2(ctx, getter, name, 0, JS_CFUNC_getter, 0) : JS_UNDEFINED;
	JSValue set_fn = setter ? JS_NewCFunction2(ctx, setter, name, 1, JS_CFUNC_setter, 0) : JS_UNDEFINED;
	int flags = JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE;
	if (getter)
		flags |= JS_PROP_HAS_GET;
	if (setter)
		flags |= JS_PROP_HAS_SET;
	JS_DefineProperty(ctx, proto, atom, JS_UNDEFINED, get_fn, set_fn, flags);
	JS_FreeValue(ctx, get_fn);
	JS_FreeValue(ctx, set_fn);
	JS_FreeAtom(ctx, atom);
}

static JSValue js_doc_get_body(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	(void)this_val;
	if (!gl_ctx.dom_root)
		return JS_NULL;
	HtmlDom_t *body = dom_query_selector(gl_ctx.dom_root, "body");
	return create_js_dom_wrapper(ctx, body ? body : gl_ctx.dom_root);
}

static JSValue js_doc_get_head(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	(void)this_val;
	if (!gl_ctx.dom_root)
		return JS_NULL;
	HtmlDom_t *head = dom_query_selector(gl_ctx.dom_root, "head");
	return head ? create_js_dom_wrapper(ctx, head) : JS_NULL;
}

static JSValue js_doc_get_document_element(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
	(void)this_val;
	if (!gl_ctx.dom_root)
		return JS_NULL;
	HtmlDom_t *html = dom_query_selector(gl_ctx.dom_root, "html");
	return create_js_dom_wrapper(ctx, html ? html : gl_ctx.dom_root);
}

static void qjs_proto_init(JSContext *ctx, JSRuntime *rt) {
#if defined(USE_QUICKJS_NG) || defined(QUICKJS_NG)
	JS_NewClassID(rt, &gl_ctx.js_class_id);
	JS_NewClassID(rt, &gl_ctx.observer_class_id);
	JS_NewClassID(rt, &gl_ctx.canvas_class_id);
#else
	JS_NewClassID(&gl_ctx.js_class_id);
	JS_NewClassID(&gl_ctx.observer_class_id);
	JS_NewClassID(&gl_ctx.canvas_class_id);
#endif
	JS_NewClass(rt, gl_ctx.js_class_id, &js_dom_node_class);
	JS_NewClass(rt, gl_ctx.observer_class_id, &js_dom_obs_class);
	JS_NewClass(rt, gl_ctx.canvas_class_id, &js_dom_canvas_class);

	JSValue proto = JS_NewObject(ctx);

	/* Standard DOM Node/Element Properties */
	define_accessor(ctx, proto, "tagName",               js_dom_get_tag_name,               NULL);
	define_accessor(ctx, proto, "nodeName",              js_dom_get_node_name,              NULL);
	define_accessor(ctx, proto, "nodeType",              js_dom_get_node_type,              NULL);
	define_accessor(ctx, proto, "id",                    js_dom_get_id,                     js_dom_set_id);
	define_accessor(ctx, proto, "className",             js_dom_get_class_name,             js_dom_set_class_name);
	define_accessor(ctx, proto, "textContent",           js_dom_get_text_content,           js_dom_set_text_content);
	define_accessor(ctx, proto, "innerText",             js_dom_get_text_content,           js_dom_set_text_content);
	define_accessor(ctx, proto, "innerHTML",             js_dom_get_inner_html,             js_dom_set_inner_html);
	define_accessor(ctx, proto, "outerHTML",             js_dom_get_outer_html,             js_dom_set_outer_html);
	define_accessor(ctx, proto, "parentNode",            js_dom_get_parent_node,            NULL);
	define_accessor(ctx, proto, "parentElement",         js_dom_get_parent_node,            NULL);
	define_accessor(ctx, proto, "childNodes",            js_dom_get_child_nodes,            NULL);
	define_accessor(ctx, proto, "children",              js_dom_get_children,               NULL);
	define_accessor(ctx, proto, "firstChild",            js_dom_get_first_child,            NULL);
	define_accessor(ctx, proto, "lastChild",             js_dom_get_last_child,             NULL);
	define_accessor(ctx, proto, "nextSibling",           js_dom_get_next_sibling,           NULL);
	define_accessor(ctx, proto, "previousSibling",       js_dom_get_previous_sibling,       NULL);
	define_accessor(ctx, proto, "firstElementChild",     js_dom_get_first_element_child,    NULL);
	define_accessor(ctx, proto, "lastElementChild",      js_dom_get_last_element_child,     NULL);
	define_accessor(ctx, proto, "nextElementSibling",    js_dom_get_next_element_sibling,   NULL);
	define_accessor(ctx, proto, "previousElementSibling",js_dom_get_previous_element_sibling,NULL);
	define_accessor(ctx, proto, "length",                js_dom_get_length,                 NULL);
	define_accessor(ctx, proto, "childElementCount",     js_dom_get_child_element_count,    NULL);
	define_accessor(ctx, proto, "onclick",               js_dom_get_onclick,                js_dom_set_onclick);

	/* Standard W3C Node Type Constants on Node prototype */
	JS_SetPropertyStr(ctx, proto, "ELEMENT_NODE",  JS_NewInt32(ctx, 1));
	JS_SetPropertyStr(ctx, proto, "TEXT_NODE",     JS_NewInt32(ctx, 3));
	JS_SetPropertyStr(ctx, proto, "COMMENT_NODE",  JS_NewInt32(ctx, 8));
	JS_SetPropertyStr(ctx, proto, "DOCUMENT_NODE", JS_NewInt32(ctx, 9));

	/* DOM Mutation & Manipulation Methods */
	JS_SetPropertyStr(ctx, proto, "appendChild",
		JS_NewCFunction(ctx, js_dom_append_child, "appendChild", 1));
	JS_SetPropertyStr(ctx, proto, "insertBefore",
		JS_NewCFunction(ctx, js_dom_insert_before, "insertBefore", 2));
	JS_SetPropertyStr(ctx, proto, "removeChild",
		JS_NewCFunction(ctx, js_dom_remove_child, "removeChild", 1));
	JS_SetPropertyStr(ctx, proto, "replaceChild",
		JS_NewCFunction(ctx, js_dom_replace_child, "replaceChild", 2));
	JS_SetPropertyStr(ctx, proto, "remove",
		JS_NewCFunction(ctx, js_dom_remove, "remove", 0));
	JS_SetPropertyStr(ctx, proto, "cloneNode",
		JS_NewCFunction(ctx, js_dom_clone_node, "cloneNode", 1));
	JS_SetPropertyStr(ctx, proto, "hasChildNodes",
		JS_NewCFunction(ctx, js_dom_has_child_nodes, "hasChildNodes", 0));

	/* Attribute Methods */
	JS_SetPropertyStr(ctx, proto, "setAttribute",
		JS_NewCFunction(ctx, js_dom_set_attribute, "setAttribute", 2));
	JS_SetPropertyStr(ctx, proto, "getAttribute",
		JS_NewCFunction(ctx, js_dom_get_attribute, "getAttribute", 1));
	JS_SetPropertyStr(ctx, proto, "hasAttribute",
		JS_NewCFunction(ctx, js_dom_has_attribute, "hasAttribute", 1));
	JS_SetPropertyStr(ctx, proto, "removeAttribute",
		JS_NewCFunction(ctx, js_dom_remove_attribute, "removeAttribute", 1));

	/* Query Methods on Elements */
	JS_SetPropertyStr(ctx, proto, "querySelector",
		JS_NewCFunction(ctx, js_dom_query_selector, "querySelector", 1));
	JS_SetPropertyStr(ctx, proto, "querySelectorAll",
		JS_NewCFunction(ctx, js_dom_query_selector_all, "querySelectorAll", 1));
	JS_SetPropertyStr(ctx, proto, "getElementsByTagName",
		JS_NewCFunction(ctx, js_dom_get_elements_by_tag_name, "getElementsByTagName", 1));
	JS_SetPropertyStr(ctx, proto, "getElementsByClassName",
		JS_NewCFunction(ctx, js_dom_get_elements_by_class_name, "getElementsByClassName", 1));

	/* Event Methods */
	JS_DefinePropertyValueStr(ctx, proto, "addEventListener",
		JS_NewCFunction(ctx, js_dom_add_eventlistener, "addEventListener", 2),
		JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_DefinePropertyValueStr(ctx, proto, "removeEventListener",
		JS_NewCFunction(ctx, js_dom_remove_eventlistener, "removeEventListener", 2),
		JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_SetPropertyStr(ctx, proto, "dispatchEvent",
		JS_NewCFunction(ctx, js_dom_dispatch_event, "dispatchEvent", 1));

	JS_SetClassProto(ctx, gl_ctx.canvas_class_id, JS_DupValue(ctx, proto));
	JS_SetClassProto(ctx, gl_ctx.js_class_id, proto);
}

static void qjs_interfaces_set(JSContext *ctx, HtmlDom_t *root) {
	JSValue global = JS_GetGlobalObject(ctx);

	/* Document object backed by dom_root */
	JSValue doc_obj = create_js_dom_wrapper(ctx, root);
	define_accessor(ctx, doc_obj, "body",            js_doc_get_body,             NULL);
	define_accessor(ctx, doc_obj, "head",            js_doc_get_head,             NULL);
	define_accessor(ctx, doc_obj, "documentElement", js_doc_get_document_element, NULL);

	JS_SetPropertyStr(ctx, doc_obj, "getElementById",
		JS_NewCFunction(ctx, js_dom_get_element_by_id, "getElementById", 1));
	JS_SetPropertyStr(ctx, doc_obj, "getElementsByTagName",
		JS_NewCFunction(ctx, js_dom_get_elements_by_tag_name, "getElementsByTagName", 1));
	JS_SetPropertyStr(ctx, doc_obj, "getElementsByClassName",
		JS_NewCFunction(ctx, js_dom_get_elements_by_class_name, "getElementsByClassName", 1));
	JS_SetPropertyStr(ctx, doc_obj, "querySelector",
		JS_NewCFunction(ctx, js_dom_query_selector, "querySelector", 1));
	JS_SetPropertyStr(ctx, doc_obj, "querySelectorAll",
		JS_NewCFunction(ctx, js_dom_query_selector_all, "querySelectorAll", 1));
	JS_SetPropertyStr(ctx, doc_obj, "createElement",
		JS_NewCFunction(ctx, js_dom_create_element, "createElement", 1));
	JS_SetPropertyStr(ctx, doc_obj, "createTextNode",
		JS_NewCFunction(ctx, js_dom_create_text_node, "createTextNode", 1));
	JS_SetPropertyStr(ctx, doc_obj, "createComment",
		JS_NewCFunction(ctx, js_dom_create_comment, "createComment", 1));

	JS_SetPropertyStr(ctx, global, "document", doc_obj);
	JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

	/* Event & CustomEvent constructors */
	JSValue event_ctor = JS_NewCFunction2(ctx, js_event_ctor, "Event", 2, JS_CFUNC_constructor, 0);
	JS_SetPropertyStr(ctx, global, "Event", JS_DupValue(ctx, event_ctor));
	JS_SetPropertyStr(ctx, global, "CustomEvent", event_ctor);

	/* Console hook to capture output into js_buf
	JSValue console_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, console_obj, "log",
		JS_NewCFunction(ctx, qjs_console_hook, "log", 1));
	JS_SetPropertyStr(ctx, console_obj, "warn",
		JS_NewCFunction(ctx, qjs_console_hook, "warn", 1));
	JS_SetPropertyStr(ctx, console_obj, "error",
		JS_NewCFunction(ctx, qjs_console_hook, "error", 1));
	JS_SetPropertyStr(ctx, console_obj, "info",
		JS_NewCFunction(ctx, qjs_console_hook, "info", 1));
	JS_SetPropertyStr(ctx, global, "console", console_obj);*/

	JS_SetPropertyStr(ctx, global, "fetch",
		JS_NewCFunction(ctx, js_global_fetch, "fetch", 1));

	JS_SetPropertyStr(ctx, global, "setTimeout",
		JS_NewCFunction(ctx, js_global_set_timeout, "setTimeout", 2));

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
	gl_ctx.js_obs = obs_proto;

	JSValue bridge_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, bridge_obj, "postMessage",
		JS_NewCFunction(ctx, js_bow_post_message, "postMessage", 2));
	JS_SetPropertyStr(ctx, global, "bow", bridge_obj);

	JS_FreeValue(ctx, global);
}

/* Create QuickJS interpreter state */
bool qjs_init(bow_t web) {
	if (!web)
		return false;

	if (web->qjs && web->qjs->ctx)
		qjs_free(web);

	if (!web->qjs) {
		gl_ctx.web = web;
		web->qjs = &gl_ctx;
		if (!web->ats) {
			XInitThreads();
			XtToolkitThreadInitialize();
			XrmInitialize();
		}

		g_run_pipeline = true;
		pthread_t thread;
		pthread_mutex_init(&gl_ctx.task_mutex, NULL);
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
		js_std_free_handlers(web->qjs->rt);
		JS_FreeRuntime(web->qjs->rt);
		web->qjs->rt = NULL;
		web->qjs = NULL;
		g_run_pipeline = false;
		webview_debug("%s", "Failed to create QuickJS context.");
		return false;
	}

	JS_Canvas *cctx = js_mallocz(web->qjs->ctx, sizeof(JS_Canvas));
	if (!cctx) {
		js_std_free_handlers(web->qjs->rt);
		JS_FreeContext(web->qjs->ctx);
		JS_FreeRuntime(web->qjs->rt);
		web->qjs->rt = NULL;
		web->qjs = NULL;
		g_run_pipeline = false;
		webview_debug("%s", "Failed to create `JS_Canvas` context.");
		return false;
	}

	if (gl_ctx.canvas) {
		free(gl_ctx.canvas);
	}

	gl_ctx.canvas = cctx;
	js_init_module_std(web->qjs->ctx, "std");
	js_init_module_os(web->qjs->ctx, "os");
	js_std_add_helpers(web->qjs->ctx, 0, NULL);

	/* Add `bow_t` instance */
	JS_SetContextOpaque(web->qjs->ctx, web);
	web->qjs->js_obs = JS_UNDEFINED;
	web->qjs->js_this = JS_NewObject(web->qjs->ctx);
	if (JS_IsException(web->qjs->js_this)) {
		js_std_free_handlers(web->qjs->rt);
		JS_FreeContext(web->qjs->ctx);
		JS_FreeRuntime(web->qjs->rt);
		free(gl_ctx.canvas);
		gl_ctx.canvas = NULL;
		web->qjs->rt = NULL;
		web->qjs->ctx = NULL;
		web->qjs = NULL;
		g_run_pipeline = false;
		return false;
	}

	if (gl_ctx.dom_root) {
		js_free_dom(gl_ctx.dom_root);
	}
	gl_ctx.dom_root = dom_create_element("body");
	qjs_proto_init(web->qjs->ctx, web->qjs->rt);
	qjs_interfaces_set(web->qjs->ctx, gl_ctx.dom_root);
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

static const char *
strip_file_url_prefix(const char *u) {
	if (!u)
		return NULL;
	if (strncmp(u, "file://", 7) == 0) {
		u += 7;
		/* Strip optional host (e.g. "localhost") before the leading '/' */
		if (strncmp(u, "localhost/", 10) == 0)
			u += 9; /* keep leading '/' */
	} else if (strncmp(u, "file:", 5) == 0) {
		u += 5;
	}
	return u;
}

static void resolve_script_path(bow_t web, const char *src, char *out, size_t out_sz) {
	const char *burl = NULL;
	const char *p, *slash;
	size_t dir_len;

	if (!src || !out || out_sz == 0)
		return;

	src = strip_file_url_prefix(src);

	/* Absolute filesystem path or remote URL — use directly */
	if (*src == '/' || strstr(src, "://") != NULL) {
		snprintf(out, out_sz, "%s", src);
		return;
	}

	/* Extract directory from active document base URL */
	if (web && web->active_html)
		burl = ((HTMLInfo)web->active_html)->burl;

	p = strip_file_url_prefix(burl);
	if (p) {
		slash = strrchr(p, '/');
		if (slash != NULL) {
			dir_len = (size_t)(slash - p + 1);
			if (dir_len + strlen(src) + 1 < out_sz) {
				memcpy(out, p, dir_len);
				snprintf(out + dir_len, out_sz - dir_len, "%s", src);
				return;
			}
		}
	}

	snprintf(out, out_sz, "%s", src);
}

FORCEINLINE char *qjs_get_file(const char *path_url) {
	if (!path_url || strlen(path_url) == 0)
		return NULL;

	char f[PATH_MAX] = {0};
	resolve_script_path(gl_ctx.web, path_url, f, PATH_MAX);
	return (strncmp(path_url, "http", 4) == 0) ? qjs_fetchfile(path_url) : qjs_readfile(f);
}

HtmlDom_t *qjs_parse(bow_t web, const char *html) {
	if (!web || !html)
		return NULL;

	if (!web->qjs || !web->qjs->ctx) {
		if (!qjs_init(web))
			return NULL;
	}

	HtmlDom_t *parsed = html_parse(html);
	if (!parsed)
		return NULL;

	/* Replace previous DOM root cleanly instead of nesting a NODE_DOCUMENT inside <body> */
	if (web->qjs->dom_root) {
		js_free_dom(web->qjs->dom_root);
	}
	web->qjs->dom_root = parsed;

	/* Re-bind document wrapper to the newly parsed DOM root */
	JSValue global = JS_GetGlobalObject(web->qjs->ctx);
	JSValue doc_obj = JS_GetPropertyStr(web->qjs->ctx, global, "document");
	if (JS_IsObject(doc_obj)) {
		JS_SetOpaque(doc_obj, parsed);
	}
	JS_FreeValue(web->qjs->ctx, doc_obj);
	JS_FreeValue(web->qjs->ctx, global);

	if (web->js_data && web->js_data != (void *)html) {
		free(web->js_data);
	}
	web->js_data = strdup(html);

	return parsed;
}

static void execute_script_blocks(JSContext *ctx, HtmlDom_t *n) {
	if (!ctx || !n)
		return;

	HtmlNodeList *scripts = dom_get_elements_by_tag_id(n, TAG_SCRIPT);
	if (!scripts)
		return;

	size_t i;
	for (i = 0; i < scripts->count; i++) {
		HtmlDom_t *script_elem = scripts->items[i];
		const char *type_attr = dom_get_attr(script_elem, "type");
		const char *src_attr = dom_get_attr(script_elem, "src");
		bool is_mod = (type_attr && strcasecmp(type_attr, "module") == 0);

		/* Skip non-JS script types like application/ld+json */
		if (type_attr && !is_mod &&
		    strcasecmp(type_attr, "text/javascript") != 0 &&
		    strcasecmp(type_attr, "application/javascript") != 0 &&
		    strcasecmp(type_attr, "application/ecmascript") != 0 &&
		    strcasecmp(type_attr, "text/ecmascript") != 0) {
			continue;
		}

		if (src_attr && *src_attr) {
			char *ext_code = qjs_get_file(src_attr);
			if (ext_code) {
				run_javascript(ctx, ext_code, is_mod || qjs_is_module(src_attr));
				free(ext_code);
			}
		} else {
			char *inline_code = dom_get_raw_content(script_elem);
			if (inline_code && *inline_code) {
				run_javascript(ctx, inline_code, is_mod);
			}
			free(inline_code);
		}
	}

	dom_node_list_free(scripts);
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

FORCEINLINE void qjs_run_script(bow_t web, HtmlDom_t *parsed, const char *js, bool is_module) {
	if (!web || !web->qjs || !web->qjs->ctx)
		return;

	if (!parsed && js) {
		run_javascript(web->qjs->ctx, js, is_module);
	} else if (parsed) {
		execute_script_blocks(web->qjs->ctx, parsed);
	}
}

/* Destroy QuickJS interpreter state */
void qjs_free(bow_t web) {
	if (!web || !web->qjs || !web->qjs->ctx)
		return;

	js_free_dom(web->qjs->dom_root);
	web->qjs->dom_root = NULL;

	qjs_free_timers(web->qjs->ctx);
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
		bool is_mod = is_file && qjs_is_module(script);
		JSValue result = JS_Eval(web->qjs->ctx, file, strlen(file),
			is_file ? script : "<input>",
			is_mod ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
#if defined(USE_QUICKJS_NG) || defined(QUICKJS_NG)
		r = js_std_loop(web->qjs->ctx);
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

	qjs_free(web);
}
