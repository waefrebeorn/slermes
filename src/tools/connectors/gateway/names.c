/*
 * names.c - C11 port of upstream tools/connectors/gateway/names.py
 * (gate w24_names_184724).
 *
 * PoP: w24_names_184724_is_connector_name @ tools/connectors/gateway/names.py:is_connector_name
 * PoP: w24_names_184724_parse_connector_name @ tools/connectors/gateway/names.py:parse_connector_name
 * PoP: w24_names_184724_format_connector_name @ tools/connectors/gateway/names.py:format_connector_name
 * PoP: w24_names_184724_vendor_slug_candidates @ tools/connectors/gateway/names.py:vendor_slug_candidates
 * PoP: w24_names_184724_constants @ tools/connectors/gateway/names.py:<module constants>
 *
 * What is ported is the whole codec: composing a bridge name, splitting one
 * back into (connector, tool) with a bounded split so a slug keeps its internal
 * underscores, the prefix test the model-facing router does, and the two vendor
 * slug candidates a lookup tries in order.  The source module has no I/O and
 * neither does this: the dataclass the parser returns is a plain object.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_names_184724.h"

/* CONNECTOR_NAME_PREFIX */
#define NM_PREFIX "connectors__"
/* CONNECTOR_BATCH_SENTINEL */
#define NM_SENTINEL "connectors__execute"
/* The prefix head the split must see: "connectors". */
#define NM_PREFIX_HEAD "connectors"

/* __all__, in source order. */
static const char *const NM_ALL[7] = {
    "CONNECTOR_BATCH_SENTINEL", "CONNECTOR_NAME_PREFIX", "ConnectorName",
    "format_connector_name", "is_connector_name", "parse_connector_name",
    "vendor_slug_candidates",
};

static char *nm_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str.upper() over the ASCII alphabet (vendor slugs are ASCII by contract). */
static char *nm_upper(const char *s) {
    char *p = nm_dup(s);
    if (!p) return NULL;
    for (char *q = p; *q; q++)
        if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 'a' + 'A');
    return p;
}

static int nm_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

/* --- Python str() over a JSON value -------------------------------------
 * An f-string interpolates with str(): None -> "None", True -> "True", a
 * number in its shortest round-trip text, a string verbatim, a list/dict in
 * repr form.  Only the shapes an f-string slot can hold are needed. */
typedef struct { char *b; size_t n, cap; int oom; } nm_buf;

static void nm_bput(nm_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 64;
        while (cap < s->n + len + 1) cap *= 2;
        char *nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void nm_bputs(nm_buf *s, const char *t) { nm_bput(s, t, strlen(t)); }

/* repr() of a float: shortest text that round-trips (what Python prints). */
static void nm_num_text(double v, char *out, size_t cap) {
    char tmp[64];
    int p;
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(out, cap, "%.0f", v);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

static void nm_repr(nm_buf *s, const json_t *v);

static void nm_repr_str(nm_buf *s, const char *t) {
    nm_bput(s, "'", 1);
    for (const char *p = t; *p; p++) {
        if (*p == '\'') nm_bputs(s, "\\'");
        else if (*p == '\\') nm_bputs(s, "\\\\");
        else if (*p == '\n') nm_bputs(s, "\\n");
        else if (*p == '\t') nm_bputs(s, "\\t");
        else nm_bput(s, p, 1);
    }
    nm_bput(s, "'", 1);
}

static void nm_repr(nm_buf *s, const json_t *v) {
    size_t i, n;
    char num[64];
    if (!v) { nm_bputs(s, "None"); return; }
    switch (v->type) {
    case JSON_NULL: nm_bputs(s, "None"); return;
    case JSON_BOOL: nm_bputs(s, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: nm_num_text(v->num_val, num, sizeof(num)); nm_bputs(s, num); return;
    case JSON_STRING: nm_repr_str(s, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        nm_bput(s, "[", 1);
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) nm_bputs(s, ", ");
            nm_repr(s, json_get(v, i));
        }
        nm_bput(s, "]", 1);
        return;
    case JSON_OBJECT:
        nm_bput(s, "{", 1);
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) nm_bputs(s, ", ");
            nm_repr_str(s, json_object_get_key_at(v, i));
            nm_bputs(s, ": ");
            nm_repr(s, json_object_get_at(v, i));
        }
        nm_bput(s, "}", 1);
        return;
    }
}

/* str(value) -- a top-level string is returned verbatim, everything else
 * through repr().  Caller frees. */
static char *nm_pystr(const json_t *v) {
    nm_buf s = {NULL, 0, 0, 0};
    if (v && v->type == JSON_STRING) return nm_dup(v->str_val ? v->str_val : "");
    nm_repr(&s, v);
    if (s.oom) { free(s.b); return NULL; }
    if (!s.b) return nm_dup("");
    return s.b;
}

static json_t *nm_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

json_t *w24_names_184724_is_connector_name(const json_t *name) {
    if (!nm_is_str(name)) return json_bool(0);
    return json_bool(strncmp(name->str_val ? name->str_val : "", NM_PREFIX,
                             sizeof(NM_PREFIX) - 1) == 0);
}

json_t *w24_names_184724_parse_connector_name(const json_t *name) {
    const char *s, *p1, *p2;
    size_t plen, clen, tlen;
    char *connector, *tool;
    json_t *out;

    if (!nm_is_str(name)) return json_null();
    s = name->str_val ? name->str_val : "";

    /* name.split("__", 2): at most two splits, left to right. */
    p1 = strstr(s, "__");
    if (!p1) return json_null();          /* len(parts) != 3 */
    p2 = strstr(p1 + 2, "__");
    if (!p2) return json_null();          /* len(parts) != 3 */

    plen = (size_t)(p1 - s);
    clen = (size_t)(p2 - (p1 + 2));
    tlen = strlen(p2 + 2);
    if (plen != sizeof(NM_PREFIX_HEAD) - 1 || memcmp(s, NM_PREFIX_HEAD, plen) != 0)
        return json_null();               /* prefix != "connectors" */
    if (clen == 0 || tlen == 0) return json_null();

    connector = (char *)malloc(clen + 1);
    tool = (char *)malloc(tlen + 1);
    if (!connector || !tool) {
        free(connector); free(tool);
        return json_null();
    }
    memcpy(connector, p1 + 2, clen); connector[clen] = '\0';
    memcpy(tool, p2 + 2, tlen); tool[tlen] = '\0';

    out = json_object();
    json_set(out, "raw", json_string(s));
    json_set(out, "connector", json_string(connector));
    json_set(out, "tool", json_string(tool));
    free(connector); free(tool);
    return out;
}

json_t *w24_names_184724_format_connector_name(const json_t *connector, const json_t *tool) {
    const char *c, *t, *body;
    char *up, *head, *buf;
    size_t need;
    json_t *out;

    /* connector.upper() / tool.removeprefix on a non-str is AttributeError. */
    if (!nm_is_str(connector) || !nm_is_str(tool)) return nm_error("AttributeError");
    c = connector->str_val ? connector->str_val : "";
    t = tool->str_val ? tool->str_val : "";
    up = nm_upper(c);
    if (!up) return nm_error("MemoryError");
    need = strlen(up) + 2;
    head = (char *)malloc(need);
    if (!head) { free(up); return nm_error("MemoryError"); }
    snprintf(head, need, "%s_", up);
    free(up);

    /* tool = tool.removeprefix(f"{connector.upper()}_") -- one occurrence. */
    body = t;
    if (strncmp(t, head, strlen(head)) == 0) body = t + strlen(head);
    free(head);

    need = sizeof(NM_PREFIX) - 1 + strlen(c) + 2 + strlen(body) + 1;
    buf = (char *)malloc(need);
    if (!buf) return nm_error("MemoryError");
    snprintf(buf, need, "%s%s__%s", NM_PREFIX, c, body);
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w24_names_184724_vendor_slug_candidates(const json_t *connector, const json_t *tool) {
    const char *c;
    char *up, *first = NULL, *t;
    size_t need;
    json_t *out;

    /* f"{connector.upper()}_{tool}": a non-str connector has no .upper()
     * (AttributeError); the tool is stringified by the f-string, so any value
     * interpolates. */
    if (!nm_is_str(connector)) return nm_error("AttributeError");
    c = connector->str_val ? connector->str_val : "";
    t = nm_pystr(tool);
    if (!t) return nm_error("MemoryError");
    up = nm_upper(c);
    if (!up) return nm_error("MemoryError");
    need = strlen(up) + 1 + strlen(t) + 1;
    first = (char *)malloc(need);
    if (first) snprintf(first, need, "%s_%s", up, t);
    free(up);
    if (!first) return nm_error("MemoryError");

    out = json_array();
    json_append(out, json_string(first));
    json_append(out, tool ? json_copy(tool) : json_null());
    free(first);
    return out;
}

json_t *w24_names_184724_constants(void) {
    json_t *o = json_object();
    json_t *all = json_array();
    size_t i;
    json_set(o, "CONNECTOR_NAME_PREFIX", json_string(NM_PREFIX));
    json_set(o, "CONNECTOR_BATCH_SENTINEL", json_string(NM_SENTINEL));
    for (i = 0; i < sizeof(NM_ALL) / sizeof(NM_ALL[0]); i++) json_append(all, json_string(NM_ALL[i]));
    json_set(o, "__all__", all);
    return o;
}
