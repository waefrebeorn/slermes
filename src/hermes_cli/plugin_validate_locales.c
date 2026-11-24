/*
 * plugin_validate_locales.c -- real C11 port of the language-pack admission
 * decisions of upstream Hermes hermes_cli/plugin_validate_locales.py.
 *
 * PoP: w22_pluginvali_a951b0_locale_label @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * PoP: w22_pluginvali_a951b0_flatten_keys @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * PoP: w22_pluginvali_a951b0_non_text_leaves @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * PoP: w22_pluginvali_a951b0_known_ids @ hermes_cli/plugin_validate_locales.py:check_language_packs
 * PoP: w22_pluginvali_a951b0_check_document @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 *
 * Host seams, injected by the caller: the parsed locale document (the YAML read
 * and its parse failure), the English reference key set (`reference_keys`) and
 * the report sink. `check_language_packs`' directory scan and the
 * `hermes plugins validate` command stay upstream.
 */
#include "hermes_json.h"
#include "noc_w22_pluginvali_a951b0.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PV_CORE_SURFACE "core"

/* ---- small dynamic string list ------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} pv_list;

static int pv_push(pv_list *v, const char *s) {
    char *dup;
    if (v->count == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 8;
        char **grown = (char **)realloc(v->items, cap * sizeof(char *));
        if (!grown) return 0;
        v->items = grown;
        v->cap = cap;
    }
    dup = (char *)malloc(strlen(s) + 1);
    if (!dup) return 0;
    memcpy(dup, s, strlen(s) + 1);
    v->items[v->count++] = dup;
    return 1;
}

static void pv_list_free(pv_list *v) {
    size_t i;
    for (i = 0; i < v->count; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static int pv_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *pv_list_json(const pv_list *v) {
    json_t *out = json_array();
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < v->count; i++) json_append(out, json_string(v->items[i]));
    return out;
}

/* ---- growing text buffer (always NUL-terminated, empty path included) ---- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} pv_buf;

static void pv_buf_init(pv_buf *b) {
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

static int pv_buf_put(pv_buf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 32;
        while (cap < b->len + n + 1) cap *= 2;
        {
            char *grown = (char *)realloc(b->buf, cap);
            if (!grown) return 0;
            b->buf = grown;
            b->cap = cap;
        }
    }
    memcpy(b->buf + b->len, s, n + 1);
    b->len += n;
    return 1;
}

static const char *pv_buf_text(pv_buf *b) {
    if (!b->buf) {
        if (!pv_buf_put(b, "")) return "";
    }
    return b->buf;
}

static void pv_buf_free(pv_buf *b) {
    free(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

static void pv_join(pv_buf *b, const char *sep, const char *item, int first) {
    if (!first) pv_buf_put(b, sep);
    pv_buf_put(b, item);
}

/* ---- language ids -------------------------------------------------------- */

/* Python str.strip(): ASCII whitespace plus the extra code points str.isspace()
 * also strips (0x1c-0x1f, NBSP). Returns a caller-freed copy. */
static char *pv_strip_dup(const char *s) {
    const char *start;
    const char *end;
    size_t n;
    char *out;

    if (!s) s = "";
    start = s;
    while (*start) {
        unsigned char c = (unsigned char)*start;
        if (c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1c && c <= 0x1f)) start++;
        else if (c == 0xc2 && (unsigned char)start[1] == 0xa0) start += 2;
        else break;
    }
    end = start + strlen(start);
    while (end > start) {
        unsigned char c = (unsigned char)end[-1];
        if (c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1c && c <= 0x1f)) end--;
        else if (end - start >= 2 && (unsigned char)end[-2] == 0xc2 && c == 0xa0) end -= 2;
        else break;
    }
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

/* normalize_language_id: str -> strip, lower-case, '_' -> '-'; else "". */
static char *pv_normalize_id(const json_t *value) {
    char *text;
    char *p;
    if (!value || value->type != JSON_STRING) {
        text = (char *)malloc(1);
        if (text) text[0] = '\0';
        return text;
    }
    text = pv_strip_dup(value->str_val);
    if (!text) return NULL;
    for (p = text; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        else if (*p == '_') *p = '-';
    }
    return text;
}

/* is_language_id: ^[a-z]{2,3}(-[a-z0-9]{2,8})*$ over the normalized id. */
static int pv_is_language_id(const char *id) {
    const char *p = id;
    int letters = 0;
    if (!p) return 0;
    while (*p >= 'a' && *p <= 'z') {
        letters++;
        p++;
    }
    if (letters < 2 || letters > 3) return 0;
    while (*p == '-') {
        int n = 0;
        p++;
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
            n++;
            p++;
        }
        if (n < 2 || n > 8) return 0;
    }
    return *p == '\0';
}

char *w22_pluginvali_a951b0_locale_label(const char *lang_id, const char *surface) {
    const char *lang = lang_id ? lang_id : "";
    const char *surf = surface ? surface : "";
    pv_buf b;
    char *out;

    pv_buf_init(&b);
    pv_buf_put(&b, "locale ");
    pv_buf_put(&b, lang);
    if (strcmp(surf, PV_CORE_SURFACE) != 0) {
        pv_buf_put(&b, ".");
        pv_buf_put(&b, surf);
    }
    out = (char *)malloc(b.len + 1);
    if (out) memcpy(out, pv_buf_text(&b), b.len + 1);
    pv_buf_free(&b);
    return out;
}

/* ---- flatten / non_text_leaves ------------------------------------------ */

static void pv_flatten(const json_t *node, const char *prefix, pv_list *out) {
    size_t i;
    if (node && node->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(node); i++) {
            const char *key = json_object_get_key_at(node, i);
            json_t *child = json_object_get_at(node, i);
            pv_buf path;
            if (!key) continue;
            pv_buf_init(&path);
            if (prefix[0]) {
                pv_buf_put(&path, prefix);
                pv_buf_put(&path, ".");
            }
            pv_buf_put(&path, key);
            pv_flatten(child, pv_buf_text(&path), out);
            pv_buf_free(&path);
        }
    } else if (node && node->type == JSON_STRING) {
        pv_push(out, prefix);
    }
}

static void pv_bad_leaves(const json_t *node, const char *prefix, pv_list *out) {
    size_t i;
    if (node && node->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(node); i++) {
            const char *key = json_object_get_key_at(node, i);
            json_t *child = json_object_get_at(node, i);
            pv_buf path;
            if (!key) continue;
            pv_buf_init(&path);
            if (prefix[0]) {
                pv_buf_put(&path, prefix);
                pv_buf_put(&path, ".");
            }
            pv_buf_put(&path, key);
            pv_bad_leaves(child, pv_buf_text(&path), out);
            pv_buf_free(&path);
        }
    } else if (!node || node->type != JSON_STRING) {
        pv_push(out, prefix[0] ? prefix : "<root>");
    }
}

json_t *w22_pluginvali_a951b0_flatten_keys(const json_t *document) {
    pv_list found = {0};
    json_t *out;
    pv_flatten(document, "", &found);
    qsort(found.items, found.count, sizeof(char *), pv_cmp_str);
    out = pv_list_json(&found);
    pv_list_free(&found);
    return out;
}

json_t *w22_pluginvali_a951b0_non_text_leaves(const json_t *document) {
    pv_list found = {0};
    json_t *out;
    pv_bad_leaves(document, "", &found);
    qsort(found.items, found.count, sizeof(char *), pv_cmp_str);
    out = pv_list_json(&found);
    pv_list_free(&found);
    return out;
}

json_t *w22_pluginvali_a951b0_known_ids(const json_t *raw) {
    pv_list ids = {0};
    json_t *seq = NULL;
    json_t *out;
    size_t i;

    if (!raw || raw->type == JSON_NULL) return json_array();
    if (raw->type == JSON_STRING || raw->type == JSON_OBJECT) {
        seq = json_array();
        if (!seq) return json_array();
        json_append(seq, json_copy(raw));
    } else if (raw->type == JSON_ARRAY) {
        seq = json_copy(raw);
    } else {
        return json_array();  /* not a list: the module warns and returns [] */
    }
    if (!seq) return json_array();
    for (i = 0; i < json_len(seq); i++) {
        json_t *item = json_get(seq, i);
        char *id = NULL;
        size_t j;
        int seen = 0;
        if (item && item->type == JSON_OBJECT) id = pv_normalize_id(json_obj_get(item, "id"));
        else id = pv_normalize_id(item);
        if (!id) continue;
        if (pv_is_language_id(id)) {
            for (j = 0; j < ids.count; j++) {
                if (!strcmp(ids.items[j], id)) { seen = 1; break; }
            }
            if (!seen) pv_push(&ids, id);
        }
        free(id);
    }
    json_free(seq);
    out = pv_list_json(&ids);
    pv_list_free(&ids);
    return out;
}

/* ---- per-file verdict ---------------------------------------------------- */

static const char *pv_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return (isfinite(v->num_val) && floor(v->num_val) == v->num_val)
                             ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static int pv_reference_has(const json_t *reference, const char *key) {
    size_t i;
    for (i = 0; i < json_len(reference); i++) {
        json_t *item = json_get(reference, i);
        if (item && item->type == JSON_STRING && !strcmp(item->str_val, key)) return 1;
    }
    return 0;
}

json_t *w22_pluginvali_a951b0_check_document(const json_t *document, const char *lang_id,
                                             const char *surface, const json_t *reference,
                                             const char *filename, const char *parse_error) {
    char *label = w22_pluginvali_a951b0_locale_label(lang_id, surface);
    const char *name = filename ? filename : "";
    const char *surf = surface ? surface : "";
    json_t *out = json_object();
    json_t *warnings = json_array();
    pv_buf detail;
    int ok = 0;

    pv_buf_init(&detail);
    if (!out || !warnings || !label) {
        free(label);
        json_free(out);
        json_free(warnings);
        pv_buf_free(&detail);
        return NULL;
    }

    if (parse_error) {
        pv_buf_put(&detail, name);
        pv_buf_put(&detail, " failed to parse: ");
        pv_buf_put(&detail, parse_error);
        ok = 0;
    } else {
        /* `if document is None: document = {}` -- null/absent is an empty dict. */
        json_t *owned_empty = NULL;
        const json_t *doc = (document && document->type != JSON_NULL) ? document : NULL;
        if (!doc) {
            owned_empty = json_object();
            doc = owned_empty;
        }
        if (doc && doc->type != JSON_OBJECT) {
            pv_buf_put(&detail, name);
            pv_buf_put(&detail, " must be a mapping, got ");
            pv_buf_put(&detail, pv_typename(doc));
            ok = 0;
        } else {
            pv_list bad = {0};
            pv_bad_leaves(doc, "", &bad);
            if (bad.count) {
                size_t i;
                qsort(bad.items, bad.count, sizeof(char *), pv_cmp_str);
                pv_buf_put(&detail, name);
                pv_buf_put(&detail, " has non-text value(s) at: ");
                for (i = 0; i < bad.count; i++) pv_join(&detail, ", ", bad.items[i], i == 0);
                ok = 0;
                pv_list_free(&bad);
            } else {
                pv_list flat = {0};
                size_t i;
                pv_flatten(doc, "", &flat);
                qsort(flat.items, flat.count, sizeof(char *), pv_cmp_str);
                if (!reference || reference->type != JSON_ARRAY) {
                    char count[32];
                    snprintf(count, sizeof(count), "%lu", (unsigned long)flat.count);
                    pv_buf_put(&detail, name);
                    pv_buf_put(&detail, ": ");
                    pv_buf_put(&detail, count);
                    pv_buf_put(&detail, " key(s); no ");
                    pv_buf_put(&detail, surf);
                    pv_buf_put(&detail, " key reference available, key check skipped");
                    ok = 1;
                } else {
                    pv_list unknown = {0};
                    for (i = 0; i < flat.count; i++) {
                        if (!pv_reference_has(reference, flat.items[i]))
                            pv_push(&unknown, flat.items[i]);
                    }
                    if (unknown.count) {
                        pv_buf wbuf;
                        char count[32];
                        pv_buf_init(&wbuf);
                        pv_buf_put(&wbuf, name);
                        pv_buf_put(&wbuf, ": ");
                        snprintf(count, sizeof(count), "%lu", (unsigned long)unknown.count);
                        pv_buf_put(&wbuf, count);
                        pv_buf_put(&wbuf, " key(s) not in the English ");
                        pv_buf_put(&wbuf, surf);
                        pv_buf_put(&wbuf, " catalog (ignored at runtime): ");
                        for (i = 0; i < unknown.count; i++)
                            pv_join(&wbuf, ", ", unknown.items[i], i == 0);
                        json_append(warnings, json_string(pv_buf_text(&wbuf)));
                        pv_buf_free(&wbuf);
                    }
                    {
                        char counts[64];
                        pv_buf_put(&detail, name);
                        pv_buf_put(&detail, ": ");
                        snprintf(counts, sizeof(counts), "%lu key(s), %lu match the English ",
                                 (unsigned long)flat.count,
                                 (unsigned long)(flat.count - unknown.count));
                        pv_buf_put(&detail, counts);
                        pv_buf_put(&detail, surf);
                        pv_buf_put(&detail, " catalog");
                    }
                    ok = 1;
                    pv_list_free(&unknown);
                }
                pv_list_free(&flat);
            }
        }
        json_free(owned_empty);
    }

    json_set(out, "label", json_string(label));
    json_set(out, "ok", json_bool(ok ? 1 : 0));
    json_set(out, "detail", json_string(pv_buf_text(&detail)));
    json_set(out, "warnings", warnings);
    free(label);
    pv_buf_free(&detail);
    return out;
}
