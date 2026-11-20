/* learning_graph_flow2.c — FLOW + leaf port of agent/learning_graph_render.py.
 *
 * Ports the SIXTEEN unclaimed functions of agent/learning_graph_render.py as
 * internal `lg2_*` statics (NO PoP re-claim — every one is unclaimed, verified
 * by a CRLF-aware PoP grep of src/):
 *   _lerp, _is_memory, _node_id, _utc, _lead_in, _visible_count,
 *   _node_raw_label, _node_ts, _node_card, _skill_category_counts,
 *   ChartBucket.category, _period, _fill_even_bins, node_row, bucket_row,
 *   _bar_lengths.
 *
 * Claimed siblings are NOT re-implemented here; they are INJECTED through the
 * universal seam (io->call): format_date (claimed -> learning_graph_render_helpers.c)
 * and memory_node_id (claimed -> learning_graph_helpers.c, provided to node_row
 * via the fixture's `memory_lookup` map so no seam is needed). Trivial pure
 * helpers (_clamp) are transcribed inline.
 *
 * io->call(io, name, args) appends {"name","args"} to io->trace and returns the
 * scripted result (NULL/json_null == None; {"__raise__":cls,"message":m} == a
 * raised exception). Seam results are caller-owned; io->trace/io->state are
 * harness-owned. lg2_call copies args into the trace; the caller frees args.
 *
 * Self-contained: libjson only (hermes_json.h) + libc + libm. No filesystem,
 * no libyaml. Full builtin exception hierarchy transcribed for raise matching.
 *
 * Representation contract (shared with tests/oracle_learning_graph_flow2.py):
 *  - A node is a dict with string `kind`/`id`/`label`/`category`; ASCII scope
 *    for str/strip/len/slice (glyphs are verbatim UTF-8 constants).
 *  - A bucket is {"label":str,"ts":num,"rec":num,"nodes":[...]}.
 *  - memory_lookup is an id -> card map (body/glyph materialized by fixtures).
 *  - _utc projects to [year,month,day,hour,minute,second] (null on out-of-range
 *    is a scripted raise). _period projects to [key_array, label].
 *  - _fill_even_bins emits the mutated bucket array via io.state["buckets"].
 *  - Integral floats normalize to ints on both sides; fixtures avoid integral
 *    float literals where a str() distinction would matter and non-finite
 *    floats. round() is Python banker's rounding.
 */
#ifndef LEARNING_GRAPH_FLOW2_C
#define LEARNING_GRAPH_FLOW2_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include "hermes_json.h"

#define LG2_LEAD_IN 0.06

typedef struct lg2_io_t {
    json_t *(*call)(struct lg2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} lg2_io_t;

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *lg2_call(lg2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    return r ? r : json_null();
}

static bool lg2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *lg2_exc_class(const json_t *r) {
    if (!lg2_raised(r)) return "";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *lg2_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* Full builtin exception hierarchy (child -> parent). */
typedef struct { const char *child; const char *parent; } lg2_exc_edge_t;
static const lg2_exc_edge_t LG2_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
    {"Exception", "BaseException"}, {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"}, {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"LookupError", "Exception"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"MemoryError", "Exception"}, {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"OSError", "Exception"},
    {"EnvironmentError", "OSError"}, {"IOError", "OSError"},
    {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"}, {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"SystemError", "Exception"}, {"TypeError", "Exception"},
    {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"Warning", "Exception"},
    {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"ResourceWarning", "Warning"},
    {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"}, {NULL, NULL}
};
static const char *lg2_exc_parent(const char *cls) {
    for (size_t i = 0; LG2_EXC_PARENTS[i].child; i++)
        if (strcmp(LG2_EXC_PARENTS[i].child, cls) == 0) return LG2_EXC_PARENTS[i].parent;
    return NULL;
}
/* True if cls descends from Exception (caught by `except Exception`). */
static bool lg2_exc_is_exception(const char *cls) {
    if (!cls || !*cls) return false;
    if (strcmp(cls, "BaseException") == 0) return false;
    const char *cur = cls;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        const char *p = lg2_exc_parent(cur);
        if (!p) return true;  /* unknown leaf: assume Exception subclass */
        cur = p;
    }
    return true;
}
static bool lg2_is_base_exc(const json_t *r) {
    return lg2_raised(r) && !lg2_exc_is_exception(lg2_exc_class(r));
}
static bool lg2_is_exception(const json_t *r) {
    return lg2_raised(r) && lg2_exc_is_exception(lg2_exc_class(r));
}

/* ── primitives ────────────────────────────────────────────────────────── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return false;
}
static double jnum(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static const char *jstr(const json_t *v, const char *d) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : d;
}
static double lg2_clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v < hi ? v : hi);
}
/* Python round(): banker's rounding (ties -> even). */
static double py_round(double x) {
    double f = floor(x);
    double r = x - f;
    if (r > 0.5) return f + 1.0;
    if (r < 0.5) return f;
    return (fmod(f, 2.0) == 0.0) ? f : f + 1.0;
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return strdup(buf);
    }
    return json_serialize(v);
}
static char *ascii_strip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    memcpy(out, s, n); out[n] = '\0';
    return out;
}

/* ── _utc: UTC timestamp -> civil components [y,m,d,h,mi,s] ───────────── */
/* Howard Hinnant's civil_from_days. */
static void lg2_civil_from_days(long long z, int *yy, unsigned *mm, unsigned *dd) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp + (mp < 10 ? 3 : (unsigned)-9);
    *yy = (int)(y + (m <= 2)); *mm = m; *dd = d;
}
/* Returns array or a __raise__ object. */
static json_t *lg2_utc(double ts) {
    if (!isfinite(ts) || fabs(ts) > 8.0e13)
        return lg2_raise("OverflowError", "timestamp out of range");
    double fl = floor(ts);
    long long days = (long long)floor(fl / 86400.0);
    long long sod = (long long)(fl - (double)days * 86400.0);
    int y; unsigned mo, d;
    lg2_civil_from_days(days, &y, &mo, &d);
    if (y < 1 || y > 9999)
        return lg2_raise("OverflowError", "year out of range");
    int hh = (int)(sod / 3600), mi = (int)((sod % 3600) / 60), ss = (int)(sod % 60);
    json_t *r = json_array();
    json_append(r, json_number(y)); json_append(r, json_number(mo));
    json_append(r, json_number(d)); json_append(r, json_number(hh));
    json_append(r, json_number(mi)); json_append(r, json_number(ss));
    return r;
}
static const char *LG2_MONTHS[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};

/* ── leaf helpers ──────────────────────────────────────────────────────── */
static bool lg2_is_memory(const json_t *node) {
    return node && strcmp(jstr(json_obj_get((json_t *)node, "kind"), ""), "memory") == 0;
}
static char *lg2_node_id_str(const json_t *node) {
    if (!node) return strdup("");
    json_t *v = json_obj_get((json_t *)node, "id");
    if (!v) return strdup("");          /* absent key -> str("") == "" */
    if (v->type == JSON_NULL) return strdup("None");  /* present null -> str(None) */
    return py_str(v);
}
static char *lg2_node_raw_label_str(const json_t *node) {
    const json_t *lab = node ? json_obj_get((json_t *)node, "label") : NULL;
    const json_t *id = node ? json_obj_get((json_t *)node, "id") : NULL;
    const char *raw = "unknown";
    char *tmp = NULL;
    if (jtruth(lab)) { tmp = py_str(lab); raw = tmp; }
    else if (jtruth(id)) { tmp = py_str(id); raw = tmp; }
    else tmp = strdup("unknown");
    char *out = ascii_strip(raw);
    free(tmp);
    return out;
}
/* _node_ts: true + value if a float, false for None/conversion failure. */
static bool lg2_node_ts_val(const json_t *node, double *out) {
    const json_t *v = node ? json_obj_get((json_t *)node, "timestamp") : NULL;
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return false;
        char *end = NULL; double x = strtod(s, &end);
        if (end == s) return false;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return false;
        *out = x; return true;
    }
    return false;
}

/* _node_card: returns object or a propagated __raise__ from format_date. */
static json_t *lg2_node_card(lg2_io_t *io, const json_t *node) {
    bool mem = lg2_is_memory(node);
    char *text = lg2_node_raw_label_str(node);
    double ts = 0.0; bool have_ts = lg2_node_ts_val(node, &ts);
    json_t *args = json_object();
    json_set(args, "ts", have_ts ? json_number(ts) : json_null());
    json_t *date = lg2_call(io, "format_date", args);
    if (lg2_raised(date)) { free(text); return date; }
    const char *dates = (date->type == JSON_STRING && date->str_val) ? date->str_val : "None";
    char *meta;
    if (mem) {
        const char *src = jstr(json_obj_get((json_t *)node, "memorySource"), "");
        const char *prefix = strcmp(src, "profile") == 0 ? "profile memory" : "memory";
        size_t n = strlen(prefix) + strlen(" · ") + strlen(dates) + 1;
        meta = malloc(n); snprintf(meta, n, "%s · %s", prefix, dates);
    } else {
        char *cat = py_str(json_obj_get((json_t *)node, "category"));
        if (!jtruth(json_obj_get((json_t *)node, "category"))) { free(cat); cat = strdup("skill"); }
        long long count = 0;
        const json_t *uc = json_obj_get((json_t *)node, "useCount");
        if (jtruth(uc)) {
            if (uc->type == JSON_NUMBER) count = (long long)uc->num_val;
            else if (uc->type == JSON_STRING) count = strtoll(uc->str_val, NULL, 10);
            else if (uc->type == JSON_BOOL) count = uc->bool_val ? 1 : 0;
        }
        char xcount[64]; if (count) snprintf(xcount, sizeof xcount, "x%lld", count);
        bool pinned = jtruth(json_obj_get((json_t *)node, "pinned"));
        size_t n = strlen(cat) + strlen(" · ") + strlen(dates) + 1 +
                   (count ? strlen(" · ") + strlen(xcount) : 0) + (pinned ? strlen(" · pinned") : 0);
        meta = malloc(n);
        snprintf(meta, n, "%s · %s%s%s%s", cat, dates,
                 count ? " · " : "", count ? xcount : "", pinned ? " · pinned" : "");
        free(cat);
    }
    char *label;
    if (strlen(text) <= 26) label = strdup(text);
    else {
        size_t cut = 23;
        while (cut > 0 && isspace((unsigned char)text[cut - 1])) cut--;
        size_t n = cut + strlen("…") + 1;
        label = malloc(n); snprintf(label, n, "%.*s…", (int)cut, text);
    }
    json_t *r = json_object();
    json_set_hdr(r, "glyph", mem ? "◆" : "●");
    json_set_hdr(r, "label", label);
    json_set_hdr(r, "meta", meta);
    json_set_hdr(r, "style", mem ? "memory" : "skill");
    free(label); free(meta); free(text);
    json_free(date);
    return r;
}

/* _skill_category_counts -> JSON object {cat:count}. */
static json_t *lg2_skill_category_counts(const json_t *nodes) {
    json_t *counts = json_object();
    if (!nodes || nodes->type != JSON_ARRAY) return counts;
    for (size_t i = 0; i < json_len(nodes); i++) {
        json_t *n = json_get(nodes, i);
        if (lg2_is_memory(n)) continue;
        char *cat = py_str(json_obj_get(n, "category"));
        if (!jtruth(json_obj_get(n, "category"))) { free(cat); cat = strdup("skill"); }
        json_t *have = json_obj_get(counts, cat);
        json_set(counts, cat, json_number((have ? jnum(have, 0) : 0) + 1));
        free(cat);
    }
    return counts;
}
/* ChartBucket.category(): first key with max count (insertion order). */
static char *lg2_bucket_category(const json_t *bucket) {
    const json_t *nodes = bucket ? json_obj_get((json_t *)bucket, "nodes") : NULL;
    json_t *counts = lg2_skill_category_counts(nodes);
    char *best = NULL; double bestv = -1;
    for (size_t i = 0; i < json_object_size(counts); i++) {
        const char *k = json_object_get_key_at(counts, i);
        json_t *v = json_object_get_at(counts, i);
        double cv = jnum(v, 0);
        if (!best || cv > bestv) { best = strdup(k ? k : ""); bestv = cv; }
    }
    json_free(counts);
    return best; /* NULL when empty */
}

/* _period -> [key_array, label]. */
static json_t *lg2_period(double ts, const char *gran) {
    json_t *utc = lg2_utc(ts);
    if (lg2_raised(utc)) return utc;
    int y = (int)jnum(json_get(utc, 0), 0);
    int mo = (int)jnum(json_get(utc, 1), 0);
    int d = (int)jnum(json_get(utc, 2), 0);
    json_free(utc);
    const char *g = (gran && *gran) ? gran : "year";
    json_t *key = json_array();
    char label[64];
    if (strcmp(g, "day") == 0) {
        json_append(key, json_number(y)); json_append(key, json_number(mo)); json_append(key, json_number(d));
        snprintf(label, sizeof label, "%d %s", d, LG2_MONTHS[mo - 1]);
    } else if (strcmp(g, "month") == 0) {
        json_append(key, json_number(y)); json_append(key, json_number(mo));
        snprintf(label, sizeof label, "%s %d", LG2_MONTHS[mo - 1], y);
    } else {
        json_append(key, json_number(y));
        snprintf(label, sizeof label, "%04d", y < 0 ? 0 : y);
    }
    json_t *r = json_array();
    json_append(r, key);
    json_append(r, json_string(label));
    return r;
}

/* node_row nested helper: returns object or propagated raise. */
static json_t *lg2_node_row(lg2_io_t *io, const json_t *node, const json_t *lookup) {
    json_t *card = lg2_node_card(io, node);
    if (lg2_raised(card)) return card;
    char *id = lg2_node_id_str(node);
    char *full = lg2_node_raw_label_str(node);
    const char *body = "";
    char *body_owned = NULL;
    json_t *m = lookup ? json_obj_get((json_t *)lookup, id) : NULL;
    if (jtruth(m)) {
        const json_t *b = json_obj_get(m, "body");
        body_owned = py_str(b); /* missing body -> "None"? no: missing -> __null -> "None" */
        if (!b) { free(body_owned); body_owned = strdup(""); }
        body = body_owned;
    }
    json_t *r = json_object();
    json_set_hdr(r, "id", id);
    json_set_hdr(r, "glyph", jstr(json_obj_get(card, "glyph"), ""));
    json_set_hdr(r, "label", jstr(json_obj_get(card, "label"), ""));
    json_set_hdr(r, "fullLabel", full);
    json_set_hdr(r, "meta", jstr(json_obj_get(card, "meta"), ""));
    json_set_hdr(r, "body", body);
    json_set_hdr(r, "style", jstr(json_obj_get(card, "style"), ""));
    free(id); free(full); free(body_owned); json_free(card);
    return r;
}
/* stable insertion sort of node pointers by (node_ts or bucket_ts). */
/* sort key: Python `_node_ts(n) or bucket.ts` -> 0.0 is FALSY, so it falls back. */
static double lg2_sort_key(const json_t *n, double fallback) {
    double v;
    if (!lg2_node_ts_val(n, &v) || v == 0.0) return fallback;
    return v;
}
static int lg2_node_sort_key_le(const json_t *a, const json_t *b, double fallback) {
    return lg2_sort_key(a, fallback) <= lg2_sort_key(b, fallback);
}
static json_t *lg2_bucket_row(lg2_io_t *io, long long idx, const json_t *bucket,
                              const json_t *cmap, const json_t *lookup) {
    char *cat = lg2_bucket_category(bucket);
    json_t *color = json_null();
    if (cat) {
        json_t *c = cmap ? json_obj_get((json_t *)cmap, cat) : NULL;
        if (c) { json_free(color); color = json_copy(c); }
    }
    double bts = jnum(json_obj_get((json_t *)bucket, "ts"), 0.0);
    json_t *args = json_object(); json_set(args, "ts", json_number(bts));
    json_t *date = lg2_call(io, "format_date", args);
    if (lg2_raised(date)) { free(cat); json_free(color); return date; }
    const char *dates = (date->type == JSON_STRING && date->str_val) ? date->str_val : "None";

    const json_t *nodes = json_obj_get((json_t *)bucket, "nodes");
    size_t nn = (nodes && nodes->type == JSON_ARRAY) ? json_len(nodes) : 0;
    json_t **arr = malloc((nn ? nn : 1) * sizeof(json_t *));
    for (size_t i = 0; i < nn; i++) arr[i] = json_get(nodes, i);
    for (size_t i = 1; i < nn; i++) {  /* stable insertion sort */
        json_t *k = arr[i]; size_t j = i;
        while (j > 0 && !lg2_node_sort_key_le(arr[j - 1], k, bts)) { arr[j] = arr[j - 1]; j--; }
        arr[j] = k;
    }
    json_t *rows = json_array();
    for (size_t i = 0; i < nn; i++) {
        json_t *row = lg2_node_row(io, arr[i], lookup);
        if (lg2_raised(row)) {
            free(arr); free(cat); json_free(color); json_free(date); json_free(rows);
            return row;
        }
        json_append(rows, row);
    }
    free(arr);
    size_t memories = 0;
    for (size_t i = 0; i < nn; i++) if (lg2_is_memory(json_get(nodes, i))) memories++;
    size_t skills = nn - memories;

    json_t *r = json_object();
    json_set(r, "index", json_number((double)idx));
    json_set_hdr(r, "label", jstr(json_obj_get((json_t *)bucket, "label"), ""));
    json_set_hdr(r, "date", dates);
    json_set(r, "skills", json_number((double)skills));
    json_set(r, "memories", json_number((double)memories));
    json_set(r, "total", json_number((double)nn));
    if (cat) json_set_hdr(r, "category", cat); else json_set(r, "category", json_null());
    json_set(r, "color", color);
    json_set(r, "nodes", rows);
    free(cat); json_free(date);
    return r;
}

/* ── dispatch ──────────────────────────────────────────────────────────── */
json_t *lg2_dispatch(lg2_io_t *io, const char *func, json_t *in) {
    if (strcmp(func, "_lerp") == 0) {
        double a = jnum(json_obj_get(in, "a"), 0), b = jnum(json_obj_get(in, "b"), 0), t = jnum(json_obj_get(in, "t"), 0);
        return json_number(a + (b - a) * t);
    }
    if (strcmp(func, "_is_memory") == 0)
        return json_bool(lg2_is_memory(json_obj_get(in, "node")));
    if (strcmp(func, "_node_id") == 0) {
        char *s = lg2_node_id_str(json_obj_get(in, "node"));
        json_t *r = json_string(s); free(s); return r;
    }
    if (strcmp(func, "_utc") == 0)
        return lg2_utc(jnum(json_obj_get(in, "ts"), 0));
    if (strcmp(func, "_lead_in") == 0)
        return json_number(LG2_LEAD_IN + (1 - LG2_LEAD_IN) * jnum(json_obj_get(in, "ratio"), 0));
    if (strcmp(func, "_visible_count") == 0) {
        double reveal = jnum(json_obj_get(in, "reveal"), 0);
        double n = jnum(json_obj_get(in, "n"), 0);
        double c = lg2_clamp(ceil(reveal * n), 0, n);
        return json_number((double)(long long)c);
    }
    if (strcmp(func, "_node_raw_label") == 0) {
        char *s = lg2_node_raw_label_str(json_obj_get(in, "node"));
        json_t *r = json_string(s); free(s); return r;
    }
    if (strcmp(func, "_node_ts") == 0) {
        double v;
        if (lg2_node_ts_val(json_obj_get(in, "node"), &v)) return json_number(v);
        return json_null();
    }
    if (strcmp(func, "_node_card") == 0)
        return lg2_node_card(io, json_obj_get(in, "node"));
    if (strcmp(func, "_skill_category_counts") == 0)
        return lg2_skill_category_counts(json_obj_get(in, "nodes"));
    if (strcmp(func, "category") == 0) {
        char *c = lg2_bucket_category(json_obj_get(in, "bucket"));
        json_t *r = c ? json_string(c) : json_null();
        free(c); return r;
    }
    if (strcmp(func, "_period") == 0)
        return lg2_period(jnum(json_obj_get(in, "ts"), 0), jstr(json_obj_get(in, "granularity"), "year"));
    if (strcmp(func, "_fill_even_bins") == 0) {
        json_t *buckets = json_obj_get(in, "buckets");
        json_t *nodes = json_obj_get(in, "nodes");
        json_t *rec = json_obj_get(in, "rec");
        json_t *rmap = rec ? json_obj_get(rec, "rec") : NULL;
        json_t *outb = json_array();
        size_t nb = (buckets && buckets->type == JSON_ARRAY) ? json_len(buckets) : 0;
        for (size_t i = 0; i < nb; i++) json_append(outb, json_copy(json_get(buckets, i)));
        size_t nn = (nodes && nodes->type == JSON_ARRAY) ? json_len(nodes) : 0;
        for (size_t i = 0; i < nn; i++) {
            json_t *node = json_get(nodes, i);
            char *id = lg2_node_id_str(node);
            json_t *rv = rmap ? json_obj_get(rmap, id) : NULL;
            double ratio = rv ? jnum(rv, 0.0) : 0.0;
            free(id);
            long long idx = (long long)lg2_clamp(floor(ratio * (double)(nb ? nb : 1)), 0, (double)(nb ? nb - 1 : 0));
            if (nb && idx >= 0 && idx < (long long)nb) {
                json_t *b = json_get(outb, (size_t)idx);
                json_t *bnodes = json_obj_get(b, "nodes");
                if (!bnodes || bnodes->type != JSON_ARRAY) { bnodes = json_array(); json_set(b, "nodes", bnodes); }
                json_append(bnodes, json_copy(node));
            }
        }
        if (io && io->state) json_set(io->state, "buckets", outb);
        else json_free(outb);
        return json_null();
    }
    if (strcmp(func, "node_row") == 0)
        return lg2_node_row(io, json_obj_get(in, "node"), json_obj_get(in, "memory_lookup"));
    if (strcmp(func, "bucket_row") == 0)
        return lg2_bucket_row(io, (long long)jnum(json_obj_get(in, "idx"), 0),
                              json_obj_get(in, "bucket"), json_obj_get(in, "cmap"),
                              json_obj_get(in, "memory_lookup"));
    if (strcmp(func, "_bar_lengths") == 0) {
        const json_t *bucket = json_obj_get(in, "bucket");
        const json_t *nodes = bucket ? json_obj_get((json_t *)bucket, "nodes") : NULL;
        size_t total = (nodes && nodes->type == JSON_ARRAY) ? json_len(nodes) : 0;
        size_t memories = 0;
        for (size_t i = 0; i < total; i++) if (lg2_is_memory(json_get(nodes, i))) memories++;
        size_t skills = total - memories;
        double max_total = jnum(json_obj_get(in, "max_total"), 1);
        double bar_w = jnum(json_obj_get(in, "bar_w"), 0);
        long long bar = total ? (long long)fmax(1, py_round(((double)total / (max_total ? max_total : 1)) * bar_w)) : 0;
        long long sk = skills ? (long long)fmax(1, py_round(((double)skills / (double)total) * (double)bar)) : 0;
        if (memories && sk == bar && bar > 1) sk = bar - 1;
        json_t *r = json_array();
        json_append(r, json_number((double)bar));
        json_append(r, json_number((double)sk));
        json_append(r, json_number((double)(bar - sk)));
        return r;
    }
    json_t *e = json_object();
    json_set_hdr(e, "error", "unknown func");
    return e;
}

#endif /* LEARNING_GRAPH_FLOW2_C */
