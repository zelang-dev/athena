#include "html.h"
#include <stdint.h>

struct ShadowNode_s {
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

/* ============================================================================
 * 1. Alphabetically Sorted HTML5 + Legacy Tag Table
 * ============================================================================ */

static const HTMLTagInfo HTML_TAG_TABLE[] = {
    { TAG_A,          "a",          false },
    { TAG_ABBR,       "abbr",       false },
    { TAG_ADDRESS,    "address",    false },
    { TAG_APPLET,     "applet",     false },
    { TAG_AREA,       "area",       true  },
    { TAG_ARTICLE,    "article",    false },
    { TAG_ASIDE,      "aside",      false },
    { TAG_AUDIO,      "audio",      false },
    { TAG_B,          "b",          false },
    { TAG_BASE,       "base",       true  },
    { TAG_BASEFONT,   "basefont",   true  },
    { TAG_BDI,        "bdi",        false },
    { TAG_BDO,        "bdo",        false },
    { TAG_BIG,        "big",        false },
    { TAG_BLOCKQUOTE, "blockquote", false },
    { TAG_BODY,       "body",       false },
    { TAG_BR,         "br",         true  },
    { TAG_BUTTON,     "button",     false },
    { TAG_CANVAS,     "canvas",     false },
    { TAG_CAPTION,    "caption",    false },
    { TAG_CENTER,     "center",     false },
    { TAG_CITE,       "cite",       false },
    { TAG_CODE,       "code",       false },
    { TAG_COL,        "col",        true  },
    { TAG_COLGROUP,   "colgroup",   false },
    { TAG_DATA,       "data",       false },
    { TAG_DATALIST,   "datalist",   false },
    { TAG_DD,         "dd",         false },
    { TAG_DEL,        "del",        false },
    { TAG_DETAILS,    "details",    false },
    { TAG_DFN,        "dfn",        false },
    { TAG_DIALOG,     "dialog",     false },
    { TAG_DIR,        "dir",        false },
    { TAG_DIV,        "div",        false },
    { TAG_DL,         "dl",         false },
    { TAG_DOCTYPE,    "doctype",	true  },
    { TAG_DT,         "dt",         false },
    { TAG_EM,         "em",         false },
    { TAG_EMBED,      "embed",      true  },
    { TAG_FIELDSET,   "fieldset",   false },
    { TAG_FIGCAPTION, "figcaption", false },
    { TAG_FIGURE,     "figure",     false },
    { TAG_FONT,       "font",       false },
    { TAG_FOOTER,     "footer",     false },
    { TAG_FORM,       "form",       false },
	{ TAG_FRAME,      "frame",      false },
    { TAG_FRAMESET,   "frameset",	false },
    { TAG_H1,         "h1",         false },
    { TAG_H2,         "h2",         false },
    { TAG_H3,         "h3",         false },
    { TAG_H4,         "h4",         false },
    { TAG_H5,         "h5",         false },
    { TAG_H6,         "h6",         false },
    { TAG_HEAD,       "head",       false },
    { TAG_HEADER,     "header",     false },
    { TAG_HGROUP,     "hgroup",     false },
    { TAG_HR,         "hr",         true  },
    { TAG_HTML,       "html",       false },
    { TAG_I,          "i",          false },
    { TAG_IFRAME,     "iframe",     false },
    { TAG_IMG,        "img",        true  },
    { TAG_INPUT,      "input",      true  },
    { TAG_INS,        "ins",        false },
    { TAG_ISINDEX,    "isindex",    true  },
    { TAG_KBD,        "kbd",        false },
    { TAG_LABEL,      "label",      false },
    { TAG_LEGEND,     "legend",     false },
    { TAG_LI,         "li",         false },
    { TAG_LINK,       "link",       true  },
    { TAG_LISTING,    "listing",    false },
    { TAG_MAIN,       "main",       false },
    { TAG_MAP,        "map",        false },
    { TAG_MARK,       "mark",       false },
    { TAG_MATH,       "math",       false },
    { TAG_MENU,       "menu",       false },
    { TAG_META,       "meta",       true  },
    { TAG_METER,      "meter",      false },
    { TAG_NAV,        "nav",        false },
	{ TAG_NOFRAMES,   "noframes",   false },
    { TAG_NOSCRIPT,   "noscript",   false },
    { TAG_OBJECT,     "object",     false },
    { TAG_OL,         "ol",         false },
    { TAG_OPTGROUP,   "optgroup",   false },
    { TAG_OPTION,     "option",     false },
    { TAG_OUTPUT,     "output",     false },
    { TAG_P,          "p",          false },
    { TAG_PARAM,      "param",      true  },
    { TAG_PICTURE,    "picture",    false },
    { TAG_PLAINTEXT,  "plaintext",  false },
    { TAG_PRE,        "pre",        false },
    { TAG_PROGRESS,   "progress",   false },
    { TAG_Q,          "q",          false },
    { TAG_RP,         "rp",         false },
    { TAG_RT,         "rt",         false },
    { TAG_RUBY,       "ruby",       false },
    { TAG_S,          "s",          false },
    { TAG_SAMP,       "samp",       false },
    { TAG_SCRIPT,     "script",     false },
    { TAG_SEARCH,     "search",     false },
    { TAG_SECTION,    "section",    false },
    { TAG_SELECT,     "select",     false },
    { TAG_SLOT,       "slot",       false },
    { TAG_SMALL,      "small",      false },
    { TAG_SOURCE,     "source",     true  },
    { TAG_SPAN,       "span",       false },
    { TAG_STRIKE,     "strike",     false },
    { TAG_STRONG,     "strong",     false },
    { TAG_STYLE,      "style",      false },
    { TAG_SUB,        "sub",        false },
    { TAG_SUMMARY,    "summary",    false },
    { TAG_SUP,        "sup",        false },
    { TAG_SVG,        "svg",        false },
    { TAG_TABLE,      "table",      false },
    { TAG_TBODY,      "tbody",      false },
    { TAG_TD,         "td",         false },
    { TAG_TEMPLATE,   "template",   false },
    { TAG_TEXTAREA,   "textarea",   false },
    { TAG_TFOOT,      "tfoot",      false },
    { TAG_TH,         "th",         false },
    { TAG_THEAD,      "thead",      false },
    { TAG_TIME,       "time",       false },
    { TAG_TITLE,      "title",      false },
    { TAG_TR,         "tr",         false },
    { TAG_TRACK,      "track",      true  },
    { TAG_TT,         "tt",         false },
    { TAG_U,          "u",          false },
    { TAG_UL,         "ul",         false },
    { TAG_VAR,        "var",        false },
    { TAG_VIDEO,      "video",      false },
    { TAG_WBR,        "wbr",        true  },
    { TAG_XMP,        "xmp",        false }
};

#define HTML_TAG_TABLE_SIZE (sizeof(HTML_TAG_TABLE) / sizeof(HTML_TAG_TABLE[0]))
static const HTMLTagInfo UNKNOWN_TAG_INFO = { TAG_DOCUMENT, "unknown", false };

/* ============================================================================
 * 2. String & Dynamic Buffer Helpers
 * ============================================================================ */

static int ascii_strncasecmp(const char *s1, const char *s2, size_t n) {
	size_t i;
	for (i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)tolower((unsigned char)s1[i]);
        unsigned char c2 = (unsigned char)tolower((unsigned char)s2[i]);
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return (s2[n] == '\0') ? 0 : -(int)(unsigned char)s2[n];
}

static int ascii_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        unsigned char c1 = (unsigned char)tolower((unsigned char)*s1++);
        unsigned char c2 = (unsigned char)tolower((unsigned char)*s2++);
        if (c1 != c2) return (int)c1 - (int)c2;
    }
    return (int)(unsigned char)tolower((unsigned char)*s1) -
           (int)(unsigned char)tolower((unsigned char)*s2);
}

static char *strndup_safe(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) {
        if (n > 0) memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

static char *strdup_safe(const char *s) {
    return s ? strndup_safe(s, strlen(s)) : NULL;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->cap = 256;
    sb->len = 0;
    sb->data = (char *)malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static bool sb_ensure(StrBuf *sb, size_t extra) {
    if (!sb->data) return false;
    if (sb->len + extra + 1 <= sb->cap) return true;
    size_t new_cap = sb->cap * 2;
    while (new_cap < sb->len + extra + 1) new_cap *= 2;
    char *p = (char *)realloc(sb->data, new_cap);
    if (!p) return false;
    sb->data = p;
    sb->cap = new_cap;
    return true;
}

static void sb_append_n(StrBuf *sb, const char *s, size_t n) {
    if (n == 0 || !sb_ensure(sb, n)) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_append(StrBuf *sb, const char *s) {
    if (s) sb_append_n(sb, s, strlen(s));
}

static void sb_append_char(StrBuf *sb, char c) {
    sb_append_n(sb, &c, 1);
}

/* Fast O(log N) Tag Lookup */
const HTMLTagInfo *html_lookup_tag(const char *tag_name, size_t len) {
	if (!tag_name || len == 0)
		return &UNKNOWN_TAG_INFO;

	int low = 0;
	int high = (int)HTML_TAG_TABLE_SIZE - 1;

	while (low <= high) {
		int mid = low + (high - low) / 2;
		int cmp = ascii_strncasecmp(tag_name, HTML_TAG_TABLE[mid].name, len);
		if (cmp == 0) {
			if (HTML_TAG_TABLE[mid].name[len] == '\0') {
				return &HTML_TAG_TABLE[mid];
			}
			/* Table entry has extra trailing characters, so target is smaller */
			high = mid - 1;
		} else if (cmp < 0) {
			high = mid - 1;
		} else {
			low = mid + 1;
		}
	}

	return &UNKNOWN_TAG_INFO;
}


const HTMLTagInfo *html_lookup_tag_by_id(HTMLTagID id) {
	size_t i;
	if (id <= TAG_DOCUMENT || id >= TAG_COUNT)
		return &UNKNOWN_TAG_INFO;

    if ((size_t)(id - 1) < HTML_TAG_TABLE_SIZE && HTML_TAG_TABLE[id - 1].id == id) {
        return &HTML_TAG_TABLE[id - 1];
	}

	for (i = 0; i < HTML_TAG_TABLE_SIZE; i++) {
        if (HTML_TAG_TABLE[i].id == id) return &HTML_TAG_TABLE[i];
	}

	return &UNKNOWN_TAG_INFO;
}

/* ============================================================================
 * 4. HTML Entity Decoding & Encoding
 * ============================================================================ */

typedef struct {
    const char *entity;
    const char *utf8;
} HTMLEntity;

static const HTMLEntity NAMED_ENTITIES[] = {
    { "amp",    "&"            },
    { "apos",   "'"            },
    { "bull",   "\xE2\x80\xA2" },
    { "copy",   "\xC2\xA9"     },
    { "deg",    "\xC2\xB0"     },
    { "euro",   "\xE2\x82\xAC" },
    { "gt",     ">"            },
    { "hellip", "\xE2\x80\xA6" },
    { "laquo",  "\xC2\xAB"     },
    { "ldquo",  "\xE2\x80\x9C" },
    { "lsquo",  "\xE2\x80\x98" },
    { "lt",     "<"            },
    { "mdash",  "\xE2\x80\x94" },
    { "nbsp",   "\xC2\xA0"     },
    { "ndash",  "\xE2\x80\x93" },
    { "pound",  "\xC2\xA3"     },
    { "quot",   "\""           },
    { "raquo",  "\xC2\xBB"     },
    { "rdquo",  "\xE2\x80\x9D" },
    { "reg",    "\xC2\xAE"     },
    { "rsquo",  "\xE2\x80\x99" },
    { "trade",  "\xE2\x84\xA2" },
    { "yen",    "\xC2\xA5"     }
};

static size_t encode_utf8(uint32_t cp, char *out) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

char *html_decode_entities(const char *src, size_t len) {
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t i = 0, w = 0;
    while (i < len) {
        if (src[i] == '&') {
            size_t semi = i + 1;
            while (semi < len && (semi - i) < 12 && src[semi] != ';' &&
                   src[semi] != '&' && !isspace((unsigned char)src[semi])) {
                semi++;
            }
            if (semi < len && src[semi] == ';' && semi > i + 1) {
                size_t elen = semi - (i + 1);
                const char *ent = src + i + 1;

                if (ent[0] == '#') {
                    char *endptr = NULL;
                    unsigned long cp = 0;
                    if (elen > 2 && (ent[1] == 'x' || ent[1] == 'X')) {
                        cp = strtoul(ent + 2, &endptr, 16);
                    } else if (elen > 1) {
                        cp = strtoul(ent + 1, &endptr, 10);
                    }
                    if (endptr == src + semi && cp > 0 && cp <= 0x10FFFF) {
                        w += encode_utf8((uint32_t)cp, out + w);
                        i = semi + 1;
                        continue;
                    }
                } else {
                    bool matched = false;
                    size_t k, n_ent = sizeof(NAMED_ENTITIES) / sizeof(NAMED_ENTITIES[0]);
                    for (k = 0; k < n_ent; k++) {
                        if (strlen(NAMED_ENTITIES[k].entity) == elen &&
                            strncmp(NAMED_ENTITIES[k].entity, ent, elen) == 0) {
                            size_t ulen = strlen(NAMED_ENTITIES[k].utf8);
                            memcpy(out + w, NAMED_ENTITIES[k].utf8, ulen);
                            w += ulen;
                            i = semi + 1;
                            matched = true;
                            break;
                        }
                    }
                    if (matched) continue;
                }
            }
        }
        out[w++] = src[i++];
    }
    out[w] = '\0';
    return out;
}

char *html_encode_entities(const char *src, bool is_attr) {
	StrBuf sb;
	const char *p;
	sb_init(&sb);
    if (!src) return sb.data;

    for (p = src; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '&') {
            sb_append(&sb, "&amp;");
        } else if (c == '<') {
            sb_append(&sb, "&lt;");
        } else if (c == '>') {
            sb_append(&sb, "&gt;");
        } else if (is_attr && c == '"') {
            sb_append(&sb, "&quot;");
        } else if (c == 0xC2 && (unsigned char)p[1] == 0xA0) {
            sb_append(&sb, "&nbsp;");
            p++;
        } else {
            sb_append_char(&sb, *p);
        }
    }
    return sb.data;
}

/* ============================================================================
 * 5. DOM Node Creation, Mutation & Attributes
 * ============================================================================ */

static HtmlDom_t *dom_create_node(HtmlNodeType type) {
    HtmlDom_t *n = (HtmlDom_t *)calloc(1, sizeof(HtmlDom_t));
	if (n)
		n->type = type;

	return n;
}

HtmlDom_t *dom_create_element(const char *tag_name) {
	if (!tag_name)
		return NULL;

	size_t len = strlen(tag_name);
    const HTMLTagInfo *info = html_lookup_tag(tag_name, len);
    HtmlDom_t *elem = dom_create_node(NODE_ELEMENT);
	if (!elem)
		return NULL;

	elem->tag_id = info->id;
	elem->tag_name = strndup_safe(tag_name, len);

	return elem;
}

HtmlDom_t *dom_create_text(const char *text) {
    HtmlDom_t *txt = dom_create_node(NODE_TEXT);
	if (!txt)
		return NULL;

	txt->text = strdup_safe(text ? text : "");
    return txt;
}

HtmlDom_t *dom_create_comment(const char *comment) {
    HtmlDom_t *cmt = dom_create_node(NODE_COMMENT);
	if (!cmt)
		return NULL;

	cmt->text = strdup_safe(comment ? comment : "");
	return cmt;
}

static bool dom_is_ancestor_or_self(const HtmlDom_t *ancestor, const HtmlDom_t *node) {
	const HtmlDom_t *curr = node;
	while (curr) {
		if (curr == ancestor)
			return true;

		curr = curr->parent;
	}
	return false;
}

void dom_append_child(HtmlDom_t *parent, HtmlDom_t *child) {
	if (!parent || !child)
		return;

	if (parent->type != NODE_DOCUMENT && parent->type != NODE_ELEMENT)
		return;

	if (dom_is_ancestor_or_self(child, parent))
		return;

	if (child->parent) {
		dom_remove_child(child->parent, child);
	}

	child->parent = parent;
	child->next_sibling = NULL;
	if (!parent->first_child) {
		parent->first_child = parent->last_child = child;
	} else {
		parent->last_child->next_sibling = child;
		parent->last_child = child;
	}
}

void dom_insert_before(HtmlDom_t *parent, HtmlDom_t *new_node, HtmlDom_t *ref_node) {
	if (!parent || !new_node) return;
	if (parent->type != NODE_DOCUMENT && parent->type != NODE_ELEMENT) return;
	if (!ref_node) {
		dom_append_child(parent, new_node);
		return;
	}
	if (ref_node->parent != parent)
		return;

	if (new_node == ref_node)
		return;

	if (dom_is_ancestor_or_self(new_node, parent))
		return;

	if (new_node->parent) {
		dom_remove_child(new_node->parent, new_node);
	}

	new_node->parent = parent;
	if (parent->first_child == ref_node) {
		new_node->next_sibling = ref_node;
		parent->first_child = new_node;
	} else {
		HtmlDom_t *prev = parent->first_child;
		while (prev && prev->next_sibling != ref_node) {
			prev = prev->next_sibling;
		}
		if (prev) {
			new_node->next_sibling = ref_node;
			prev->next_sibling = new_node;
		}
	}
}

/* Detach a node from its current parent if attached */
HtmlDom_t *dom_remove_child(HtmlDom_t *parent, HtmlDom_t *child) {
	if (!parent || !child || child->parent != parent)
		return NULL;

    if (parent->first_child == child) {
        parent->first_child = child->next_sibling;
        if (parent->last_child == child) {
            parent->last_child = NULL;
        }
    } else {
        HtmlDom_t *prev = parent->first_child;
        while (prev && prev->next_sibling != child) {
            prev = prev->next_sibling;
        }
        if (!prev) return NULL;
        prev->next_sibling = child->next_sibling;
        if (parent->last_child == child) {
            parent->last_child = prev;
        }
    }

    child->parent = NULL;
    child->next_sibling = NULL;
    return child;
}

HtmlDom_t *dom_replace_child(HtmlDom_t *parent, HtmlDom_t *new_child, HtmlDom_t *old_child) {
	if (!parent || !new_child || !old_child || old_child->parent != parent) return NULL;
	if (parent->type != NODE_DOCUMENT && parent->type != NODE_ELEMENT) return NULL;
	if (new_child == old_child) return old_child;
	if (dom_is_ancestor_or_self(new_child, parent)) return NULL;

	if (new_child->parent) {
		dom_remove_child(new_child->parent, new_child);
	}

	new_child->parent = parent;
	new_child->next_sibling = old_child->next_sibling;

	if (parent->first_child == old_child) {
		parent->first_child = new_child;
	} else {
		HtmlDom_t *prev = parent->first_child;
		while (prev && prev->next_sibling != old_child) {
			prev = prev->next_sibling;
		}
		if (!prev) {
			new_child->parent = NULL;
			new_child->next_sibling = NULL;
			return NULL;
		}
		prev->next_sibling = new_child;
	}

	if (parent->last_child == old_child) {
		parent->last_child = new_child;
	}

	old_child->parent = NULL;
	old_child->next_sibling = NULL;
	return old_child;
}

static void dom_add_attr_raw(HtmlDom_t *elem, const char *name, size_t nlen, const char *val, size_t vlen) {
	HtmlAttr *a = (HtmlAttr *)malloc(sizeof(HtmlAttr));
	if (!a)
		return;

	size_t i;
	a->name = strndup_safe(name, nlen);
    for (i = 0; i < nlen; i++) {
        a->name[i] = (char)tolower((unsigned char)a->name[i]);
    }
    a->value = val ? html_decode_entities(val, vlen) : strdup_safe("");
    a->next = NULL;

    if (!elem->attrs) {
        elem->attrs = a;
    } else {
        HtmlAttr *tail = elem->attrs;
		while (tail->next)
			tail = tail->next;

		tail->next = a;
    }
}

const char *dom_get_attr(const HtmlDom_t *elem, const char *name) {
	if (!elem || !name)
		return NULL;

	HtmlAttr *attr;
	for (attr = elem->attrs; attr; attr = attr->next) {
		if (ascii_strcasecmp(attr->name, name) == 0)
			return attr->value;
	}

	return NULL;
}

bool dom_has_attr(const HtmlDom_t *node, const char *name) {
	return dom_get_attr(node, name) != NULL;
}

void dom_set_attr(HtmlDom_t *elem, const char *name, const char *value) {
	if (!elem || elem->type != NODE_ELEMENT || !name || name[0] == '\0')
		return;

	HtmlAttr *a;
	for (a = elem->attrs; a; a = a->next) {
        if (ascii_strcasecmp(a->name, name) == 0) {
            free(a->value);
            a->value = strdup_safe(value ? value : "");
            return;
        }
	}

	a = (HtmlAttr *)malloc(sizeof(HtmlAttr));
	if (!a)
		return;

	size_t i;
	a->name = strdup_safe(name);
    for (i = 0; a->name[i]; i++) {
        a->name[i] = (char)tolower((unsigned char)a->name[i]);
	}

	a->value = strdup_safe(value ? value : "");
    a->next = NULL;
    if (!elem->attrs) {
        elem->attrs = a;
    } else {
        HtmlAttr *tail = elem->attrs;
        while (tail->next) tail = tail->next;
        tail->next = a;
    }
}

bool dom_remove_attr(HtmlDom_t *elem, const char *name) {
	if (!elem || elem->type != NODE_ELEMENT || !name)
		return false;

	HtmlAttr *prev = NULL;
    HtmlAttr *curr = elem->attrs;
    while (curr) {
        if (ascii_strcasecmp(curr->name, name) == 0) {
			if (prev)
				prev->next = curr->next;
			else
				elem->attrs = curr->next;
			free(curr->name);
            free(curr->value);
            free(curr);
            return true;
		}

		prev = curr;
        curr = curr->next;
    }
    return false;
}

HtmlDom_t *dom_parent(HtmlDom_t *node) {
	if (node && node->parent)
		return node->parent;

	return NULL;
}

void dom_free(HtmlDom_t *node) {
	if (!node)
		return;

	HtmlDom_t *c = node->first_child;
    while (c) {
        HtmlDom_t *next = c->next_sibling;
        dom_free(c);
        c = next;
	}

	HtmlAttr *a = node->attrs;
    while (a) {
        HtmlAttr *next = a->next;
        free(a->name);
        free(a->value);
        free(a);
        a = next;
	}

	free(node->tag_name);
    free(node->text);
    free(node);
}

/* HTML5 Parser (Including Raw Text Elements: <script>, <style>, <xmp>, etc. */

static bool is_raw_text_tag(HTMLTagID id) {
	return id == TAG_SCRIPT || id == TAG_STYLE || id == TAG_XMP
		|| id == TAG_LISTING || id == TAG_PLAINTEXT;
}

static bool is_rcdata_tag(HTMLTagID id) {
    return id == TAG_TITLE || id == TAG_TEXTAREA;
}

static const char *find_closing_tag_ci(const char *haystack, const char *tag_name) {
    size_t tlen = strlen(tag_name);
    const char *p = haystack;
    while (*p) {
        if (p[0] == '<' && p[1] == '/') {
            const char *name_start = p + 2;
			while (isspace((unsigned char)*name_start))
				name_start++;

			if (ascii_strncasecmp(name_start, tag_name, tlen) == 0) {
                char after = name_start[tlen];
                if (after == '>' || after == '/' || isspace((unsigned char)after)) {
                    return p;
                }
            }
        }
        p++;
    }
    return NULL;
}

HtmlDom_t *html_parse(const char *html) {
    HtmlDom_t *doc = dom_create_node(NODE_DOCUMENT);
	if (!doc || !html)
		return doc;

    HtmlDom_t *curr = doc;
    const char *page = html;

    while (*page) {
        if (*page == '<') {
            /* Comment */
			if (strncmp(page, "<!--", 4) == 0) {
				const char *end = strstr(page + 4, "-->");
				size_t clen = end ? (size_t)(end - (page + 4)) : strlen(page + 4);
                HtmlDom_t *cmt = dom_create_node(NODE_COMMENT);
                if (cmt) {
					cmt->text = strndup_safe(page + 4, clen);
                    dom_append_child(curr, cmt);
				}

				page = end ? end + 3 : page + strlen(page);
                continue;
			}

			/* DOCTYPE or Processing Instruction */
			if (page[1] == '!' || page[1] == '?') {
				const char *end = strchr(page + 2, '>');
				if (end && ascii_strncasecmp(page, "<!DOCTYPE", 9) == 0
					&& doc->tag_id == TAG_DOCUMENT) {
					doc->tag_id = TAG_DOCTYPE;
					free(doc->tag_name);
					doc->tag_name = strdup_safe("#document");
				}

				page = end ? end + 1 : page + strlen(page);
				continue;
			}

			/* Closing Tag */
            if (page[1] == '/') {
				page += 2;
				while (isspace((unsigned char)*page))
					page++;

				const char *start = page;
				while (*page && *page != '>' && !isspace((unsigned char)*page))
					page++;

				size_t len = (size_t)(page - start);
				const char *end = strchr(page, '>');
				page = end ? end + 1 : page + strlen(page);

                HtmlDom_t *target = curr;
                while (target && target->type == NODE_ELEMENT) {
                    if (strlen(target->tag_name) == len &&
                        ascii_strncasecmp(start, target->tag_name, len) == 0) {
                        curr = target->parent ? target->parent : doc;
                        break;
					}

					target = target->parent;
				}
				continue;
            }

            /* Opening Tag */
			page++;
			while (isspace((unsigned char)*page))
				page++;

			const char *tstart = page;
			while (*page && *page != '>' && *page != '/' && !isspace((unsigned char)*page))
				page++;

			size_t tlen = (size_t)(page - tstart);
            if (tlen == 0) {
				if (*page == '>')
					page++;
				continue;
            }

            const HTMLTagInfo *info = html_lookup_tag(tstart, tlen);
            HtmlDom_t *elem = dom_create_node(NODE_ELEMENT);
            elem->tag_id = info->id;
			elem->tag_name = strndup_safe(tstart, tlen);
			size_t i;
			for (i = 0; i < tlen; i++) {
                elem->tag_name[i] = (char)tolower((unsigned char)elem->tag_name[i]);
            }

            bool explicit_self_close = false;

            /* Parse Attributes */
            while (*page && *page != '>') {
				while (isspace((unsigned char)*page))
					page++;

				if (*page == '/' && page[1] == '>') {
                    explicit_self_close = true;
					page++;
                    break;
                }
				if (*page == '>' || !*page)
					break;

				const char *astart = page;
				while (*page && *page != '=' && *page != '>' && *page != '/' && !isspace((unsigned char)*page))
					page++;
				size_t alen = (size_t)(page - astart);
				if (alen == 0) {
					page++;
					continue;
				}

				while (isspace((unsigned char)*page))
					page++;
				if (*page == '=') {
                    page++;
					while (isspace((unsigned char)*page))
						page++;
					if (*page == '"' || *page == '\'') {
                        char quote = *page++;
						const char *vstart = page;
						while (*page && *page != quote)
							page++;
						dom_add_attr_raw(elem, astart, alen, vstart, (size_t)(page - vstart));
                        if (*page == quote) page++;
                    } else {
						const char *vstart = page;
						while (*page && *page != '>' && !isspace((unsigned char)*page))
							page++;
						dom_add_attr_raw(elem, astart, alen, vstart, (size_t)(page - vstart));
                    }
                } else {
                    dom_add_attr_raw(elem, astart, alen, NULL, 0);
                }
            }
            if (*page == '>') page++;

            dom_append_child(curr, elem);

            /* Handle Raw Text (<script>, <style>, <xmp>) & RCDATA (<title>, <textarea>) */
            if (!info->is_selfclose && !explicit_self_close) {
                if (elem->tag_id == TAG_PLAINTEXT) {
                    if (*page) {
						HtmlDom_t *txt = dom_create_text(page);
                        dom_append_child(elem, txt);
						page += strlen(page);
                    }
                } else if (is_raw_text_tag(elem->tag_id) || is_rcdata_tag(elem->tag_id)) {
					const char *close_pos = find_closing_tag_ci(page, elem->tag_name);
					size_t raw_len = close_pos ? (size_t)(close_pos - page) : strlen(page);
                    if (raw_len > 0) {
                        HtmlDom_t *txt = dom_create_node(NODE_TEXT);
                        if (is_rcdata_tag(elem->tag_id)) {
							txt->text = html_decode_entities(page, raw_len);
                        } else {
                            /* Raw text tags do NOT decode HTML entities */
							txt->text = strndup_safe(page, raw_len);
                        }
                        dom_append_child(elem, txt);
                    }
                    if (close_pos) {
                        const char *end_gt = strchr(close_pos, '>');
						page = end_gt ? end_gt + 1 : close_pos + strlen(close_pos);
                    } else {
						page += raw_len;
                    }
                } else {
                    curr = elem;
                }
            }
        } else {
            /* Normal Text Node */
			const char *tstart = page;
			while (*page && *page != '<') page++;
			size_t tlen = (size_t)(page - tstart);

            HtmlDom_t *txt = dom_create_node(NODE_TEXT);
            txt->text = html_decode_entities(tstart, tlen);
            dom_append_child(curr, txt);
        }
	}

	return doc;
}

/* Raw Tag Content & Subtree Extraction */

static void collect_text_recursive(const HtmlDom_t *node, StrBuf *sb) {
	if (!node)
		return;

	if (node->type == NODE_TEXT && node->text) {
        sb_append(sb, node->text);
        return;
	}

	HtmlDom_t *c;
	for (c = node->first_child; c; c = c->next_sibling) {
        collect_text_recursive(c, sb);
    }
}

char *dom_get_raw_content(const HtmlDom_t *node) {
    StrBuf sb;
    sb_init(&sb);
	if (!node)
		return sb.data;

    if (node->type == NODE_TEXT || node->type == NODE_COMMENT) {
        sb_append(&sb, node->text);
        return sb.data;
	}

	collect_text_recursive(node, &sb);
    return sb.data;
}

char *dom_get_text_content(const HtmlDom_t *node) {
    return dom_get_raw_content(node);
}

void dom_set_text_content(HtmlDom_t *node, const char *text) {
	HtmlDom_t *child;
	HtmlDom_t *next_child;
	if (!node) return;

	if (node->type == NODE_TEXT || node->type == NODE_COMMENT) {
		free(node->text);
		node->text = strdup_safe(text ? text : "");
		return;
	}

	/* Remove existing child nodes and set a single text node */
	child = node->first_child;
	while (child) {
		next_child = child->next_sibling;
		dom_free(child);
		child = next_child;
	}
	node->first_child = NULL;
	node->last_child = NULL;

	if (text && *text) {
		dom_append_child(node, dom_create_text(text));
	}
}

static void node_list_push(HtmlNodeList *list, HtmlDom_t *node) {
	size_t i;
	for (i = 0; i < list->count; i++) {
		if (list->items[i] == node) return;
	}

	if (list->count == list->capacity) {
		size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
		HtmlDom_t **p = (HtmlDom_t **)realloc(list->items, new_cap * sizeof(HtmlDom_t *));
		if (!p)
			return;

		list->items = p;
		list->capacity = new_cap;
	}

	list->items[list->count++] = node;
}

static void find_by_tag_id_rec(const HtmlDom_t *node, HTMLTagID tag_id, HtmlNodeList *list) {
	if (!node)
		return;

	if (node->type == NODE_ELEMENT && node->tag_id == tag_id) {
        node_list_push(list, (HtmlDom_t *)node);
	}

	HtmlDom_t *c;
	for (c = node->first_child; c; c = c->next_sibling) {
        find_by_tag_id_rec(c, tag_id, list);
    }
}

static void find_by_tag_name_rec(const HtmlDom_t *node, const char *tag_name, HtmlNodeList *list) {
	if (!node)
		return;

	if (node->type == NODE_ELEMENT && node->tag_name &&
        ascii_strcasecmp(node->tag_name, tag_name) == 0) {
        node_list_push(list, (HtmlDom_t *)node);
	}

	HtmlDom_t *c;
	for (c = node->first_child; c; c = c->next_sibling) {
        find_by_tag_name_rec(c, tag_name, list);
    }
}

HtmlNodeList *dom_get_elements_by_tag_id(const HtmlDom_t *root, HTMLTagID tag_id) {
    HtmlNodeList *list = (HtmlNodeList *)calloc(1, sizeof(HtmlNodeList));
	if (!list || !root)
		return list;

	find_by_tag_id_rec(root, tag_id, list);
    return list;
}

HtmlNodeList *dom_get_elements_by_tag_name(const HtmlDom_t *root, const char *tag_name) {
    HtmlNodeList *list = (HtmlNodeList *)calloc(1, sizeof(HtmlNodeList));
	if (!list || !root || !tag_name)
		return list;

	find_by_tag_name_rec(root, tag_name, list);
    return list;
}

void dom_node_list_free(HtmlNodeList *list) {
	if (!list)
		return;

	free(list->items);
    free(list);
}

char **dom_extract_tag_contents(const HtmlDom_t *root, HTMLTagID tag_id, size_t *out_count) {
	if (out_count)
		*out_count = 0;

	HtmlNodeList *list = dom_get_elements_by_tag_id(root, tag_id);
    if (!list || list->count == 0) {
        dom_node_list_free(list);
        return NULL;
    }

    char **results = (char **)calloc(list->count, sizeof(char *));
    if (!results) {
        dom_node_list_free(list);
        return NULL;
	}

	size_t i;
    for (i = 0; i < list->count; i++) {
        results[i] = dom_get_raw_content(list->items[i]);
	}

	if (out_count)
		*out_count = list->count;

	dom_node_list_free(list);
    return results;
}

char **dom_extract_tag_contents_by_name(const HtmlDom_t *root, const char *tag_name, size_t *out_count) {
	if (out_count)
		*out_count = 0;

	HtmlNodeList *list = dom_get_elements_by_tag_name(root, tag_name);
    if (!list || list->count == 0) {
        dom_node_list_free(list);
        return NULL;
    }

    char **results = (char **)calloc(list->count, sizeof(char *));
    if (!results) {
        dom_node_list_free(list);
        return NULL;
	}

	size_t i;
    for (i = 0; i < list->count; i++) {
        results[i] = dom_get_raw_content(list->items[i]);
	}

	if (out_count)
		*out_count = list->count;

	dom_node_list_free(list);
    return results;
}

void html_string_array_free(char **arr, size_t count) {
	if (!arr)
		return;

	size_t i;
	for (i = 0; i < count; i++) {
        free(arr[i]);
	}

	free(arr);
}

/* ============================================================================
 * 8. DOM Serialization (Pretty & Compact)
 * ============================================================================ */

static bool is_whitespace_only(const char *s) {
    if (!s) return true;
    while (*s) {
		if (!isspace((unsigned char)*s))
			return false;
		s++;
    }
    return true;
}

static void append_indent(StrBuf *sb, int depth) {
	int i;
	for (i = 0; i < depth; i++) {
        sb_append(sb, "  ");
    }
}

static void serialize_node(const HtmlDom_t *node, StrBuf *sb, bool pretty, int depth, bool in_raw) {
	if (!node)
		return;

	HtmlDom_t *c;
    if (node->type == NODE_DOCUMENT) {
        for (c = node->first_child; c; c = c->next_sibling) {
            serialize_node(c, sb, pretty, depth, false);
		}
		return;
    }

    if (node->type == NODE_COMMENT) {
		if (pretty)
			append_indent(sb, depth);

		sb_append(sb, "<!--");
        sb_append(sb, node->text ? node->text : "");
        sb_append(sb, "-->");
		if (pretty)
			sb_append_char(sb, '\n');

		return;
    }

    if (node->type == NODE_TEXT) {
		if (!node->text)
			return;

		if (in_raw) {
            sb_append(sb, node->text);
            return;
        }
		if (pretty && is_whitespace_only(node->text))
			return;

        char *encoded = html_encode_entities(node->text, false);
        if (pretty) {
			append_indent(sb, depth);

			/* Trim leading/trailing whitespace in pretty mode */
            char *start = encoded;
			while (*start && isspace((unsigned char)*start))
				start++;

			char *end = start + strlen(start);
			while (end > start && isspace((unsigned char)end[-1]))
				end--;

			sb_append_n(sb, start, (size_t)(end - start));
            sb_append_char(sb, '\n');
        } else {
            sb_append(sb, encoded);
        }
        free(encoded);
        return;
    }

    if (node->type == NODE_ELEMENT) {
        const HTMLTagInfo *info = html_lookup_tag(node->tag_name, strlen(node->tag_name));
        bool raw_tag = is_raw_text_tag(node->tag_id);

		if (pretty)
			append_indent(sb, depth);

		sb_append_char(sb, '<');
		sb_append(sb, node->tag_name);
		HtmlAttr *a;
        for (a = node->attrs; a; a = a->next) {
            sb_append_char(sb, ' ');
            sb_append(sb, a->name);
            if (a->value && a->value[0] != '\0') {
                char *enc_val = html_encode_entities(a->value, true);
                sb_append(sb, "=\"");
                sb_append(sb, enc_val);
                sb_append_char(sb, '"');
                free(enc_val);
            }
		}

		sb_append_char(sb, '>');
        if (info->is_selfclose) {
            if (pretty) sb_append_char(sb, '\n');
            return;
		}

		HtmlDom_t *c;
        if (raw_tag) {
            for (c = node->first_child; c; c = c->next_sibling) {
                serialize_node(c, sb, false, 0, true);
			}

			sb_append(sb, "</");
            sb_append(sb, node->tag_name);
            sb_append_char(sb, '>');
			if (pretty)
				sb_append_char(sb, '\n');

			return;
        }

        /* Single inline text child optimization for clean pretty output */
		if (pretty && node->first_child && node->first_child == node->last_child
			&& node->first_child->type == NODE_TEXT && !is_whitespace_only(node->first_child->text)) {
			char *encoded = html_encode_entities(node->first_child->text, false);
            char *start = encoded;
			while (*start && isspace((unsigned char)*start))
				start++;

			char *end = start + strlen(start);
			while (end > start && isspace((unsigned char)end[-1]))
				end--;

			sb_append_n(sb, start, (size_t)(end - start));
            free(encoded);

            sb_append(sb, "</");
            sb_append(sb, node->tag_name);
            sb_append(sb, ">\n");
            return;
        }

		if (pretty && node->first_child)
			sb_append_char(sb, '\n');

        for (c = node->first_child; c; c = c->next_sibling) {
            serialize_node(c, sb, pretty, depth + 1, false);
        }

		if (pretty && node->first_child)
			append_indent(sb, depth);

		sb_append(sb, "</");
        sb_append(sb, node->tag_name);
        sb_append_char(sb, '>');
		if (pretty)
			sb_append_char(sb, '\n');
	}
}

char *dom_serialize(const HtmlDom_t *node, bool pretty) {
    StrBuf sb;
    sb_init(&sb);
    serialize_node(node, &sb, pretty, 0, false);
    return sb.data;
}

char *dom_get_inner_html(const HtmlDom_t *node, bool pretty) {
    StrBuf sb;
    sb_init(&sb);
	if (!node)
		return sb.data;

	HtmlDom_t *c;
	for (c = node->first_child; c; c = c->next_sibling) {
        serialize_node(c, &sb, pretty, 0, is_raw_text_tag(node->tag_id));
	}

	return sb.data;
}

/* CSS Selector Query Engine */
typedef struct {
	char *tag;
	char *id;
	char **classes;
	size_t class_count;
	char *attr_name;
	char *attr_value;
	bool has_attr;
	bool direct_child;
} SimpleSelector;

static void simple_selector_free(SimpleSelector *sel) {
	size_t i;
	if (!sel)
		return;

	free(sel->tag);
	free(sel->id);
	for (i = 0; i < sel->class_count; i++) {
		free(sel->classes[i]);
	}

	free(sel->classes);
	free(sel->attr_name);
	free(sel->attr_value);
}

static bool token_list_contains(const char *attr_val, const char *target) {
	const char *p = attr_val;
	const char *start;
	size_t target_len;
	size_t tok_len;

	if (!attr_val || !target)
		return false;

	target_len = strlen(target);
	while (*p) {
		while (isspace((unsigned char)*p))
			p++;

		if (!*p)
			break;

		start = p;
		while (*p && !isspace((unsigned char)*p))
			p++;

		tok_len = (size_t)(p - start);
		if (tok_len == target_len && strncmp(start, target, target_len) == 0) {
			return true;
		}
	}

	return false;
}

static bool match_simple_selector(const HtmlDom_t *node, const SimpleSelector *sel) {
	size_t i;
	const char *val;

	if (!node || node->type != NODE_ELEMENT || !node->tag_name) {
		return false;
	}

	if (sel->tag && strcmp(sel->tag, "*") != 0) {
		if (ascii_strcasecmp(node->tag_name, sel->tag) != 0) {
			return false;
		}
	}

	if (sel->id) {
		val = dom_get_attr(node, "id");
		if (!val || strcmp(val, sel->id) != 0) {
			return false;
		}
	}

	if (sel->class_count > 0) {
		val = dom_get_attr(node, "class");
		if (!val)
			return false;

		for (i = 0; i < sel->class_count; i++) {
			if (!token_list_contains(val, sel->classes[i])) {
				return false;
			}
		}
	}

	if (sel->has_attr && sel->attr_name) {
		val = dom_get_attr(node, sel->attr_name);
		if (!val)
			return false;

		if (sel->attr_value && strcmp(val, sel->attr_value) != 0) {
			return false;
		}
	}

	return true;
}

static SimpleSelector *parse_selector_chain(const char *selector, size_t *out_count) {
	const char *p = selector;
	const char *start;
	SimpleSelector *chain = NULL;
	SimpleSelector cur;
	size_t count = 0;
	size_t cap = 0;
	size_t len;
	bool next_direct = false;
	char quote;

	*out_count = 0;
	if (!selector)
		return NULL;

	while (*p) {
		while (isspace((unsigned char)*p))
			p++;

		if (!*p)
			break;

		if (*p == '>') {
			next_direct = true;
			p++;
			continue;
		}

		memset(&cur, 0, sizeof(SimpleSelector));
		cur.direct_child = next_direct;
		next_direct = false;

		if (*p == '*') {
			cur.tag = strdup_safe("*");
			p++;
		} else if (isalpha((unsigned char)*p) || *p == '_' || *p == '-') {
			start = p;
			while (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
				p++;
			cur.tag = strndup_safe(start, (size_t)(p - start));
		}

		while (*p && !isspace((unsigned char)*p) && *p != '>') {
			if (*p == '#') {
				p++;
				start = p;
				while (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
					p++;

				free(cur.id);
				cur.id = strndup_safe(start, (size_t)(p - start));
			} else if (*p == '.') {
				p++;
				start = p;
				while (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
					p++;

				len = (size_t)(p - start);
				if (len > 0) {
					cur.classes = (char **)realloc(cur.classes, sizeof(char *) * (cur.class_count + 1));
					cur.classes[cur.class_count++] = strndup_safe(start, len);
				}
			} else if (*p == '[') {
				p++;
				while (isspace((unsigned char)*p))
					p++;

				start = p;
				while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != ']')
					p++;

				free(cur.attr_name);
				cur.attr_name = strndup_safe(start, (size_t)(p - start));
				cur.has_attr = true;

				while (isspace((unsigned char)*p))
					p++;

				if (*p == '=') {
					p++;
					while (isspace((unsigned char)*p))
						p++;

					if (*p == '"' || *p == '\'') {
						quote = *p++;
						start = p;
						while (*p && *p != quote)
							p++;

						free(cur.attr_value);
						cur.attr_value = strndup_safe(start, (size_t)(p - start));
						if (*p == quote)
							p++;
					} else {
						start = p;
						while (*p && !isspace((unsigned char)*p) && *p != ']')
							p++;

						free(cur.attr_value);
						cur.attr_value = strndup_safe(start, (size_t)(p - start));
					}
				}

				while (*p && *p != ']')
					p++;

				if (*p == ']')
					p++;
			} else {
				p++;
			}
		}

		if (count == cap) {
			cap = (cap == 0) ? 4 : cap * 2;
			chain = (SimpleSelector *)realloc(chain, sizeof(SimpleSelector) * cap);
		}
		chain[count++] = cur;
	}

	*out_count = count;
	return chain;
}

static bool match_chain_right_to_left(const HtmlDom_t *node, const SimpleSelector *chain, size_t count) {
	int idx;
	const HtmlDom_t *curr;
	const HtmlDom_t *anc;
	bool found_anc;

	if (!node || !chain || count == 0)
		return false;

	idx = (int)count - 1;
	curr = node;
	if (!match_simple_selector(curr, &chain[idx])) {
		return false;
	}

	while (idx > 0) {
		if (chain[idx].direct_child) {
			curr = curr->parent;
			idx--;
			if (!curr || !match_simple_selector(curr, &chain[idx])) {
				return false;
			}
		} else {
			idx--;
			anc = curr->parent;
			found_anc = false;
			while (anc && anc->type == NODE_ELEMENT) {
				if (match_simple_selector(anc, &chain[idx])) {
					curr = anc;
					found_anc = true;
					break;
				}
				anc = anc->parent;
			}
			if (!found_anc)
				return false;
		}
	}

	return true;
}

static void collect_matches(HtmlDom_t *node, const SimpleSelector *chain, size_t count, HtmlNodeList *out) {
	HtmlDom_t *child;
	if (!node)
		return;
	if (match_chain_right_to_left(node, chain, count)) {
		node_list_push(out, node);
	}

	for (child = node->first_child; child; child = child->next_sibling) {
		collect_matches(child, chain, count, out);
	}
}

HtmlNodeList *dom_query_selector_all(HtmlDom_t *root, const char *selector) {
	HtmlNodeList *list;
	const char *p = selector;
	const char *seg_start;
	char *seg;
	SimpleSelector *chain;
	size_t count, i;
	bool in_bracket = false;
	char quote = 0;

	if (!root || !selector)
		return NULL;

	list = (HtmlNodeList *)calloc(1, sizeof(HtmlNodeList));
	if (!list)
		return NULL;

	seg_start = p;
	while (1) {
		if (*p == '"' || *p == '\'') {
			if (quote == 0)
				quote = *p;
			else if (quote == *p)
				quote = 0;
		} else if (quote == 0) {
			if (*p == '[')
				in_bracket = true;
			else if (*p == ']')
				in_bracket = false;
		}

		if ((*p == ',' && !in_bracket && quote == 0) || *p == '\0') {
			seg = strndup_safe(seg_start, (size_t)(p - seg_start));
			if (seg) {
				chain = parse_selector_chain(seg, &count);
				if (chain && count > 0) {
					collect_matches(root, chain, count, list);
					for (i = 0; i < count; i++)
						simple_selector_free(&chain[i]);
					free(chain);
				}
				free(seg);
			}
			if (*p == '\0') break;
			seg_start = p + 1;
		}
		p++;
	}

	return list;
}

HtmlDom_t *dom_query_selector(HtmlDom_t *root, const char *selector) {
	HtmlNodeList *list = dom_query_selector_all(root, selector);
	HtmlDom_t *first = NULL;
	if (list) {
		if (list->count > 0) {
			first = list->items[0];
		}
		dom_node_list_free(list);
	}
	return first;
}

static bool is_only_spaces(const char *str) {
	if (str == NULL || *str == '\0')
		return true;

	while (*str != '\0') {
		if (!isspace((unsigned char)*str))
			return false;
		str++;
	}

	return true;
}

/* Debug Printing */
void dom_print(const HtmlDom_t *node, int depth) {
	int i;
	HtmlAttr *a;
	HtmlDom_t *child;

	if (!node)
		return;

	if (node->type == NODE_TEXT || node->type == NODE_COMMENT) {
		if (node->text && !is_only_spaces(node->text)) {
			for (i = 0; i < depth; i++)
				printf("  ");
			printf("\"%s\"\n", node->text);
		}
	} else if (node->type == NODE_ELEMENT) {
		for (i = 0; i < depth; i++)
			printf("  ");
		printf("<%s (id=%d)", node->tag_name, node->tag_id);

		for (a = node->attrs; a; a = a->next) {
			printf(" %s=\"%s\"", a->name, a->value);
		}

		printf(">\n");
	}

	for (child = node->first_child; child; child = child->next_sibling) {
		dom_print(child, depth + 1);
	}
}