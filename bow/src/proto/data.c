/*
 * data.c — RFC 2397 data: URL scheme handler
 */

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "Bow.h"
#include "BowSource.h"
#include "mime.h"

typedef struct {
	MemPool    mp;
	BowSource  ws;
	BowTask    wt;
	byte *data;
	size_t     len;
	MIMEHeader mh;
} DataClosure;

static char *g_webview_asset_dir = NULL;

static const int b64_table[128] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
};

void bow_assetDatadir_set(const char *dir) {
	char resolved[PATH_MAX];

	if (g_webview_asset_dir != NULL) {
		free(g_webview_asset_dir);
		g_webview_asset_dir = NULL;
	}
	if (dir != NULL && *dir != '\0') {
		if (realpath(dir, resolved) != NULL)
			g_webview_asset_dir = strdup(resolved);
		else
			g_webview_asset_dir = strdup(dir);
	}
}

static int has_url_scheme(const char *url) {
	const char *p = url;
	if (!isalpha((unsigned char)*p))
		return 0;
	while (*p && (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.'))
		p++;
	return (*p == ':');
}

static byte *decode_base64(MemPool mp, const char *src, size_t len, size_t *out_len) {
	byte *out;
	size_t i = 0, j = 0;

	out = (byte *)MPCGet(mp, (len / 4) * 3 + 4);
	while (i < len) {
		int v[4] = {0, 0, 0, 0};
		int pad = 0, k = 0;
		unsigned int triple;

		while (k < 4 && i < len) {
			unsigned char c = (unsigned char)src[i++];
			if (isspace(c)) continue;
			if (c == '=') {
				v[k++] = 0;
				pad++;
			} else if (c < 128 && b64_table[c] >= 0) {
				v[k++] = b64_table[c];
			}
		}
		if (k < 4) break;

		triple = ((unsigned int)v[0] << 18) |
			((unsigned int)v[1] << 12) |
			((unsigned int)v[2] << 6) |
			(unsigned int)v[3];

		out[j++] = (byte)((triple >> 16) & 0xFF);
		if (pad < 2) out[j++] = (byte)((triple >> 8) & 0xFF);
		if (pad < 1) out[j++] = (byte)(triple & 0xFF);
	}
	*out_len = j;
	out[j] = '\0';
	return out;
}

static byte *decode_percent(MemPool mp, const char *src, size_t len, size_t *out_len) {
	byte *out;
	size_t i = 0, j = 0;

	out = (byte *)MPCGet(mp, len + 1);
	while (i < len) {
		if (src[i] == '%' && i + 2 < len &&
			isxdigit((unsigned char)src[i + 1]) &&
			isxdigit((unsigned char)src[i + 2])) {
			char hex[3];
			hex[0] = src[i + 1];
			hex[1] = src[i + 2];
			hex[2] = '\0';
			out[j++] = (byte)strtol(hex, NULL, 16);
			i += 3;
		} else {
			out[j++] = (byte)src[i++];
		}
	}
	*out_len = j;
	out[j] = '\0';
	return out;
}

static char *data_resolve_url(MemPool mp, char *url, char *base) {
	char cwd_buf[PATH_MAX];
	const char *asset_dir;
	size_t dlen, ulen;
	char *file_url;

	(void)base;
	if (url == NULL)
		return NULL;

	/* 1. Top-level or nested data: URI */
	if (strncasecmp(url, "data:", 5) == 0)
		return MPStrDup(mp, url);

	/* 2. Already an absolute URL with another scheme (http:, file:, about:, etc.) */
	if (has_url_scheme(url))
		return MPStrDup(mp, url);

	/* 3. Absolute filesystem path -> "file:///abs/path" */
	if (url[0] == '/') {
		ulen = strlen(url);
		file_url = (char *)MPCGet(mp, 7 + ulen + 1);
		sprintf(file_url, "file://%s", url);
		return file_url;
	}

	/* 4. Relative path inside data: document -> resolve against WebView asset dir */
	asset_dir = g_webview_asset_dir;
	if (asset_dir == NULL || *asset_dir == '\0') {
		if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL)
			asset_dir = cwd_buf;
		else
			asset_dir = "/";
	}

	dlen = strlen(asset_dir);
	if (url[0] == '.' && url[1] == '/')
		url += 2;
	ulen = strlen(url);

	/* Ensure triple-slash file:///... when asset_dir starts with '/' */
	file_url = (char *)MPCGet(mp, 8 + dlen + 1 + ulen + 1);
	if (asset_dir[0] == '/') {
		if (asset_dir[dlen - 1] == '/')
			sprintf(file_url, "file://%s%s", asset_dir, url);
		else
			sprintf(file_url, "file://%s/%s", asset_dir, url);
	} else {
		if (asset_dir[dlen - 1] == '/')
			sprintf(file_url, "file:///%s%s", asset_dir, url);
		else
			sprintf(file_url, "file:///%s/%s", asset_dir, url);
	}

	return file_url;
}

static void data_deliver_task(void *closure) {
	DataClosure *dc = (DataClosure *)closure;

	dc->wt = NULL;
	SourceInit(dc->ws, false);
	SourceAdd(dc->ws);
	SourceEnd(dc->ws);
}

static void *data_init(BowSource ws, BowRequest *wr, void *class_closure) {
	MemPool mp;
	DataClosure *dc;
	const char *meta_start;
	const char *comma;
	const char *payload;
	size_t meta_len;
	size_t payload_len;
	bool is_base64 = false;
	char *content_type;
	char len_buf[32];

	(void)class_closure;

	if (wr == NULL || wr->url == NULL || strncasecmp(wr->url, "data:", 5) != 0)
		return NULL;

	meta_start = wr->url + 5;
	comma = strchr(meta_start, ',');
	if (comma == NULL)
		return NULL;

	mp = MPCreate();
	dc = (DataClosure *)MPCGet(mp, sizeof(DataClosure));
	dc->mp = mp;
	dc->ws = ws;

	meta_len = (size_t)(comma - meta_start);
	if (meta_len >= 7 && strncasecmp(comma - 7, ";base64", 7) == 0) {
		is_base64 = true;
		meta_len -= 7;
	}

	if (meta_len == 0) {
		content_type = MPStrDup(mp, "text/plain");
	} else {
		const char *semi = (const char *)memchr(meta_start, ';', meta_len);
		size_t tlen = semi ? (size_t)(semi - meta_start) : meta_len;
		if (tlen == 0) {
			content_type = MPStrDup(mp, "text/plain");
		} else {
			content_type = (char *)MPCGet(mp, tlen + 1);
			memcpy(content_type, meta_start, tlen);
			content_type[tlen] = '\0';
		}
	}

	payload = comma + 1;
	payload_len = strlen(payload);

	if (is_base64)
		dc->data = decode_base64(mp, payload, payload_len, &dc->len);
	else
		dc->data = decode_percent(mp, payload, payload_len, &dc->len);

	dc->mh = MIMECreateHeader();
	MIMEAddField(dc->mh, "x-url", wr->url);
	MIMEAddField(dc->mh, "content-type", content_type);
	sprintf(len_buf, "%lu", (unsigned long)dc->len);
	MIMEAddField(dc->mh, "content-length", len_buf);

	dc->wt = TaskSchedule(SourceToResources(ws), data_deliver_task, dc);

	return dc;
}

static void data_getdata(void *closure, byte **data, size_t *len, MIMEHeader *mh) {
	DataClosure *dc = (DataClosure *)closure;
	*data = dc->data;
	*len = dc->len;
	*mh = dc->mh;
}

static void data_stop(void *closure) {
	DataClosure *dc = (DataClosure *)closure;
	if (dc->wt != NULL) {
		TaskRemove(SourceToResources(dc->ws), dc->wt);
		dc->wt = NULL;
	}
}

static void data_destroy(void *closure) {
	DataClosure *dc = (DataClosure *)closure;
	data_stop(dc);
	if (dc->mh != NULL)
		MIMEDestroyHeader(dc->mh);
	MPDestroy(dc->mp);
}

void InitModule_Data(BowResources cres) {
	BowSourceHooks shooks;

	memset(&shooks, 0, sizeof(shooks));
	shooks.name = "data";
	shooks.init = data_init;
	shooks.stop = data_stop;
	shooks.destroy = data_destroy;
	shooks.getdata = data_getdata;
	shooks.resolve_url = data_resolve_url;

	SourceAddHooks(cres, &shooks);
}
