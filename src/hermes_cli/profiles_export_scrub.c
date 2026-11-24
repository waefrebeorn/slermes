/*
 * profiles_export_scrub.c - C11 port of upstream hermes_cli/profiles_export_scrub.py
 * (gate w25_profilesex_ccad7d).
 *
 * PoP: w25_profilesex_ccad7d_should_redact @ hermes_cli/profiles_export_scrub.py:_should_redact_export_file
 * PoP: w25_profilesex_ccad7d_masked_envelopes @ hermes_cli/profiles_export_scrub.py:_masked_envelopes
 * PoP: w25_profilesex_ccad7d_mask_envelopes_text @ hermes_cli/profiles_export_scrub.py:_mask_secret_envelopes
 * PoP: w25_profilesex_ccad7d_gateway_store_match @ hermes_cli/profiles_export_scrub.py:GATEWAY_CONNECTION_STORE_RE
 * PoP: w25_profilesex_ccad7d_constants @ hermes_cli/profiles_export_scrub.py:_EXPORT_REDACT_SUFFIXES
 *
 * The export pass itself is a seam: ``staged.rglob("*")``, the symlink
 * materialization and ``agent.redact.redact_sensitive_text`` all stay in the
 * caller.  Ported here is the decision core -- which names are text, how a
 * secret envelope is masked structurally (the text redactor cannot see key
 * names), the exact ``json.dumps(indent=2)`` re-emission that masking produces,
 * and the regex for the machine-wide saved-gateways stores.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_profilesex_ccad7d.h"

/* _EXPORT_REDACT_SUFFIXES */
static const char *const PX_SUFFIXES[] = {
    ".bash", ".cfg", ".conf", ".css", ".csv", ".html", ".ini", ".js", ".json",
    ".jsonl", ".jsx", ".md", ".py", ".sh", ".toml", ".ts", ".tsx", ".txt",
    ".xml", ".yaml", ".yml", ".zsh",
};
#define PX_NSFX (sizeof(PX_SUFFIXES) / sizeof(PX_SUFFIXES[0]))

/* _EXPORT_REDACT_NAMES */
static const char *const PX_NAMES[] = {".cursorrules"};
#define PX_NNAMES (sizeof(PX_NAMES) / sizeof(PX_NAMES[0]))

static char *px_lower(const char *s) {
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* pathlib name: the final component, trailing separators dropped. */
static void px_basename(const char *path, char *out, size_t cap) {
    size_t len = strlen(path);
    size_t start, i;
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) len--;
    start = 0;
    for (i = 0; i < len; i++) {
        if (path[i] == '/' || path[i] == '\\') start = i + 1;
    }
    if (len - start >= cap) len = start + cap - 1;
    memcpy(out, path + start, len - start);
    out[len - start] = '\0';
}

/* pathlib suffix (os.path.splitext): from the LAST dot, and only when some
 * non-dot character precedes it (".cursorrules", ".." have no suffix; "foo."
 * and "x.." do). */
static void px_suffix(const char *name, char *out, size_t cap) {
    size_t len = strlen(name);
    size_t dot, k;
    out[0] = '\0';
    if (len == 0) return;
    for (dot = len; dot > 0; dot--) {
        if (name[dot - 1] == '.') break;
    }
    if (dot == 0) return;                    /* no dot at all */
    dot--;                                   /* index of the last dot */
    for (k = 0; k < dot; k++) {
        if (name[k] == '.') continue;
        if (len - dot >= cap) return;
        memcpy(out, name + dot, len - dot);
        out[len - dot] = '\0';
        return;
    }
}

static int px_ends_with(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

static int px_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

json_t *w25_profilesex_ccad7d_should_redact(const json_t *path) {
    char name[512], suffix[256];
    char *lower;
    size_t i;
    int verdict = 0;

    if (!path || path->type != JSON_STRING || !path->str_val) return json_null();
    px_basename(path->str_val, name, sizeof name);
    px_suffix(name, suffix, sizeof suffix);
    lower = px_lower(name);
    if (!lower) return json_null();

    for (i = 0; i < PX_NNAMES; i++) {
        if (strcmp(name, PX_NAMES[i]) == 0) { verdict = 1; break; }
    }
    if (!verdict && px_ends_with(lower, ".env.example")) verdict = 1;
    if (!verdict && px_starts_with(lower, "config.yaml.")) verdict = 1;
    if (!verdict) {
        char *lsfx = px_lower(suffix);
        if (lsfx) {
            for (i = 0; i < PX_NSFX; i++) {
                if (strcmp(lsfx, PX_SUFFIXES[i]) == 0) { verdict = 1; break; }
            }
            free(lsfx);
        }
    }
    free(lower);
    return json_bool(verdict);
}

/* Python '==': absent/null are the same element, True == 1, 1 == 1.0. */
static int px_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        if (json_len(a) != json_len(b)) return 0;
        n = json_len(a);
        for (i = 0; i < n; i++)
            if (!px_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        if (json_object_size(a) != json_object_size(b)) return 0;
        n = json_object_size(a);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !px_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* set(node) == {"encoding", "value"} */
static int px_is_envelope(const json_t *node) {
    return node && node->type == JSON_OBJECT && json_object_size(node) == 2 &&
           json_obj_get(node, "encoding") && json_obj_get(node, "value");
}

json_t *w25_profilesex_ccad7d_masked_envelopes(const json_t *node) {
    size_t i, n;

    if (!node) return json_null();
    if (node->type == JSON_NULL) return json_null();
    if (node->type != JSON_ARRAY && node->type != JSON_OBJECT) return json_copy(node);

    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        n = json_len(node);
        for (i = 0; i < n; i++)
            json_append(out, w25_profilesex_ccad7d_masked_envelopes(json_get(node, i)));
        return out;
    }

    if (px_is_envelope(node)) {
        const json_t *value = json_obj_get(node, "value");
        if (value && value->type == JSON_STRING && value->str_val && value->str_val[0] != '\0') {
            json_t *masked = json_object();
            json_set(masked, "encoding", json_copy(json_obj_get(node, "encoding")));
            json_set(masked, "value", json_string("***"));
            return masked;
        }
    }
    {
        json_t *out = json_object();
        n = json_object_size(node);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(node, i);
            if (!k) continue;
            json_set(out, k, w25_profilesex_ccad7d_masked_envelopes(json_object_get_at(node, i)));
        }
        return out;
    }
}

/* ---- json.dumps(value, indent=2) equivalent for the re-emission path ---- */

typedef struct {
    char *p;
    size_t len, cap;
    int oom;
} px_buf;

static void px_buf_init(px_buf *b) {
    b->p = NULL; b->len = 0; b->cap = 0; b->oom = 0;
}

static void px_buf_need(px_buf *b, size_t extra) {
    size_t want;
    char *grown;
    if (b->oom) return;
    want = b->len + extra + 1;
    if (want <= b->cap) return;
    if (b->cap == 0) b->cap = 64;
    while (b->cap < want) b->cap *= 2;
    grown = (char *)realloc(b->p, b->cap);
    if (!grown) { b->oom = 1; return; }
    b->p = grown;
}

static void px_buf_add(px_buf *b, const char *s, size_t n) {
    px_buf_need(b, n);
    if (b->oom) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void px_buf_adds(px_buf *b, const char *s) { px_buf_add(b, s, strlen(s)); }

static void px_buf_addn(px_buf *b, size_t indent) {
    size_t i;
    px_buf_adds(b, "\n");
    px_buf_need(b, indent);
    if (b->oom) return;
    for (i = 0; i < indent; i++) b->p[b->len + i] = ' ';
    b->len += indent;
    b->p[b->len] = '\0';
}

/* One UTF-8 codepoint at s; returns bytes consumed (>=1). */
static size_t px_utf8(const unsigned char *s, unsigned long *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x0F) << 12) | ((unsigned long)(s[1] & 0x3F) << 6) |
              (s[2] & 0x3F);
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x07) << 18) | ((unsigned long)(s[1] & 0x3F) << 12) |
              ((unsigned long)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    *cp = s[0];
    return 1;
}

/* ensure_ascii=True: control chars escaped, everything >= 0x80 as \uXXXX. */
static void px_dump_string(px_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[16];
    px_buf_adds(b, "\"");
    while (*p) {
        unsigned long cp;
        size_t used;
        if (*p == '"') { px_buf_adds(b, "\\\""); p++; continue; }
        if (*p == '\\') { px_buf_adds(b, "\\\\"); p++; continue; }
        if (*p == '\n') { px_buf_adds(b, "\\n"); p++; continue; }
        if (*p == '\r') { px_buf_adds(b, "\\r"); p++; continue; }
        if (*p == '\t') { px_buf_adds(b, "\\t"); p++; continue; }
        if (*p == '\b') { px_buf_adds(b, "\\b"); p++; continue; }
        if (*p == '\f') { px_buf_adds(b, "\\f"); p++; continue; }
        used = px_utf8(p, &cp);
        if (cp < 0x20) {
            snprintf(esc, sizeof esc, "\\u%04lx", cp);
            px_buf_adds(b, esc);
        } else if (cp > 0x7E) {
            if (cp <= 0xFFFF) {
                snprintf(esc, sizeof esc, "\\u%04lx", cp);
                px_buf_adds(b, esc);
            } else {
                unsigned long v = cp - 0x10000;
                snprintf(esc, sizeof esc, "\\u%04lx\\u%04lx",
                         0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
                px_buf_adds(b, esc);
            }
        } else {
            px_buf_add(b, (const char *)p, 1);
        }
        p += used;
    }
    px_buf_adds(b, "\"");
}

/* repr() of a float: shortest of %.15g / %.16g / %.17g that round-trips. */
static void px_dump_number(px_buf *b, double v) {
    char num[64];
    if (v == (double)(long long)v && v > -1e15 && v < 1e15) {
        snprintf(num, sizeof num, "%lld", (long long)v);
    } else {
        int prec;
        for (prec = 15; prec <= 17; prec++) {
            double back;
            snprintf(num, sizeof num, "%.*g", prec, v);
            back = strtod(num, NULL);
            if (back == v) break;
        }
        if (!strpbrk(num, ".eE")) strncat(num, ".0", sizeof num - strlen(num) - 1);
    }
    px_buf_adds(b, num);
}

static void px_dump(px_buf *b, const json_t *v, size_t indent) {
    size_t i, n;
    if (!v || v->type == JSON_NULL) { px_buf_adds(b, "null"); return; }
    switch (v->type) {
    case JSON_BOOL: px_buf_adds(b, v->bool_val ? "true" : "false"); return;
    case JSON_NUMBER: px_dump_number(b, v->num_val); return;
    case JSON_STRING: px_dump_string(b, v->str_val); return;
    case JSON_ARRAY:
        n = json_len(v);
        if (n == 0) { px_buf_adds(b, "[]"); return; }
        px_buf_adds(b, "[");
        for (i = 0; i < n; i++) {
            if (i) px_buf_adds(b, ",");
            px_buf_addn(b, indent + 2);
            px_dump(b, json_get(v, i), indent + 2);
        }
        px_buf_addn(b, indent);
        px_buf_adds(b, "]");
        return;
    case JSON_OBJECT:
        n = json_object_size(v);
        if (n == 0) { px_buf_adds(b, "{}"); return; }
        px_buf_adds(b, "{");
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) px_buf_adds(b, ",");
            px_buf_addn(b, indent + 2);
            px_dump_string(b, k ? k : "");
            px_buf_adds(b, ": ");
            px_dump(b, json_object_get_at(v, i), indent + 2);
        }
        px_buf_addn(b, indent);
        px_buf_adds(b, "}");
        return;
    case JSON_NULL: px_buf_adds(b, "null"); return;
    }
}

json_t *w25_profilesex_ccad7d_mask_envelopes_text(const json_t *text) {
    json_t *doc, *masked, *out;
    px_buf b;

    if (!text || text->type != JSON_STRING || !text->str_val) return json_null();
    doc = json_parse(text->str_val, NULL);
    if (!doc) return json_string(text->str_val);        /* ValueError: untouched */
    masked = w25_profilesex_ccad7d_masked_envelopes(doc);
    if (px_equal(masked, doc)) {                        /* nothing masked: untouched */
        json_free(masked);
        json_free(doc);
        return json_string(text->str_val);
    }
    px_buf_init(&b);
    px_dump(&b, masked, 0);
    json_free(masked);
    json_free(doc);
    if (b.oom) { free(b.p); return json_null(); }
    out = json_string(b.p ? b.p : "");
    free(b.p);
    return out;
}

json_t *w25_profilesex_ccad7d_gateway_store_match(const json_t *name) {
    static const char *const head = "gateway-connection";
    const char *p;

    if (!name || name->type != JSON_STRING || !name->str_val) return json_null();
    p = name->str_val;
    if (strncmp(p, head, strlen(head)) != 0) return json_bool(0);
    p += strlen(head);
    if (*p == 's') p++;                                  /* connections? */
    if (strncmp(p, "-tokens", 7) == 0) p += 7;           /* (?:-tokens)? */
    if (strncmp(p, ".json", 5) != 0) return json_bool(0);
    p += 5;
    if (*p == '\0') return json_bool(1);
    if (*p == '.' && p[1] != '\0' && p[1] != '\n') return json_bool(1);
    return json_bool(0);
}

json_t *w25_profilesex_ccad7d_constants(void) {
    json_t *o = json_object();
    json_t *sfx = json_array();
    json_t *names = json_array();
    json_t *stores = json_array();
    size_t i;

    for (i = 0; i < PX_NSFX; i++) json_append(sfx, json_string(PX_SUFFIXES[i]));
    for (i = 0; i < PX_NNAMES; i++) json_append(names, json_string(PX_NAMES[i]));
    json_append(stores, json_string("gateway-connections.json"));
    json_append(stores, json_string("gateway-connection-tokens.json"));
    json_set(o, "redact_suffixes", sfx);
    json_set(o, "redact_names", names);
    json_set(o, "gateway_stores", stores);
    json_set(o, "gateway_store_pattern",
             json_string("gateway-connections?(?:-tokens)?\\.json(?:\\..+)?"));
    return o;
}
