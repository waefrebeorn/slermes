/* display_flow2.c — FLOW port of the 7 UNCLAIMED agent/display.py functions.
 *
 * Ports the decision chains that carried no live-name PoP claim:
 *   _preview_shell(key)          -> the shell/code preview closure builder
 *   _preview_bridge_call(name)   -> the bridge-tool preview closure builder
 *   KawaiiSpinner._skin_spinner_list
 *   KawaiiSpinner._catalog_thinking_verbs
 *   KawaiiSpinner._is_patch_stdout_proxy
 *   KawaiiSpinner.start
 *   KawaiiSpinner._clear_line_blanks
 *
 * Closure factories (_preview_shell / _preview_bridge_call) are driven as the
 * composed build(args, max_len) flow: key / tool_name is captured from `in`,
 * the builder body runs against the same scripted seams on both sides.
 *
 * Every foreign / claimed sibling sub-call is INJECTED through df2_io_t's
 * universal seam: io->call(io, name, args) appends {"name","args"} to io->trace
 * and returns the scripted result (NULL/json_null == None;
 * {"__raise__":cls,"message":msg} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * Representation contract (shared with tests/oracle_display_flow2.py):
 *  - tool label double -> {"name","emoji","text","preview"}.
 *  - skin double -> {"spinner": {key: [...]}} (or null / raise).
 *  - _out double -> {"__proxy__": true} for a StdoutProxy, anything else for
 *    an ordinary stream.
 *  - state carries running/start_time/thread/last_line_len for the spinner
 *    leaves; start() mutates running/start_time/thread in place.
 *  - Integral floats normalize to ints; fixtures avoid integral-float literals
 *    and non-finite floats. str() uses the scalar rules below (ASCII fixtures).
 *
 * Seam name table (name -> live call):
 *   sanitize_command_for_display   sanitize_command_for_display(str(command))
 *   summarize_shell_command        summarize_shell_command(sanitized)
 *   bridge_tool_labels             bridge_tool_labels(tool_name, args)
 *   _primary_arg_preview           _primary_arg_preview(tool_name, args, max_len)
 *   _tail_trunc                    _tail_trunc(text, limit)
 *   _get_skin                      _get_skin()
 *   t                              t("display.thinking_verbs")
 *   StdoutProxy_import             from prompt_toolkit.patch_stdout import StdoutProxy
 *   time.time                      time.time()
 *   threading.Thread               threading.Thread(target=self._animate, daemon=True)
 *   thread.start                   self.thread.start()
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. df2_call copies args into the trace; caller frees args.
 */
#ifndef DISPLAY_FLOW2_C
#define DISPLAY_FLOW2_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct df2_io_t {
    json_t *(*call)(struct df2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} df2_io_t;

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *df2_call(df2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    return r ? r : json_null();
}

static bool df2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *df2_exc_name(const json_t *r) {
    if (!df2_raised(r)) return "";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
/* BaseException-only trio (plus asyncio.CancelledError) propagates through
 * `except Exception` sites. */
static bool df2_is_base_exc(const json_t *r) {
    const char *s = df2_exc_name(r);
    return strcmp(s, "SystemExit") == 0 || strcmp(s, "KeyboardInterrupt") == 0
        || strcmp(s, "GeneratorExit") == 0 || strcmp(s, "CancelledError") == 0;
}
static bool df2_is_import_error(const json_t *r) {
    const char *s = df2_exc_name(r);
    return strcmp(s, "ImportError") == 0 || strcmp(s, "ModuleNotFoundError") == 0;
}

/* ── truthiness / type predicates ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static json_t *df2_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static const char *df2_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = df2_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python str() of a scalar (containers out of fixture scope). */
static char *df2_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) snprintf(buf, sizeof buf, "%.0f", x);
        else snprintf(buf, sizeof buf, "%.17g", x);
        return strdup(buf);
    }
    return strdup("");
}

/* ASCII strip (matches Python str.strip for the ASCII fixture scope). */
static bool df2_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *df2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && df2_isspace((unsigned char)p[a])) a++;
    while (b > a && df2_isspace((unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}

static const char *DF2_THINKING_VERBS[] = {
    "pondering", "contemplating", "musing", "cogitating", "ruminating",
    "deliberating", "mulling", "reflecting", "processing", "reasoning",
    "analyzing", "computing", "synthesizing", "formulating", "brainstorming",
    NULL
};

/* ===================================================================== */
/* PoP: df2_preview_shell @ agent/display.py:_preview_shell                */
/* ===================================================================== */
static json_t *df2_preview_shell(df2_io_t *io, const char *key, json_t *args, json_t *max_len) {
    json_t *cmd = df2_get(args, key);
    if (j_is_null(cmd)) return json_null();
    char *cmdstr = df2_pystr(cmd);
    json_t *a1 = json_object();
    json_set_hdr(a1, "s", cmdstr);
    json_t *san = df2_call(io, "sanitize_command_for_display", a1);
    json_free(a1);
    free(cmdstr);
    if (df2_raised(san)) return san;
    char *sanstr = df2_pystr(san);
    json_t *a2 = json_object();
    json_set_hdr(a2, "s", sanstr);
    json_t *sum = df2_call(io, "summarize_shell_command", a2);
    json_free(a2);
    free(sanstr);
    if (df2_raised(sum)) return sum;
    char *sumstr = df2_pystr(sum);
    json_t *a3 = json_object();
    json_set_hdr(a3, "text", sumstr);
    json_set(a3, "limit", max_len ? json_copy(max_len) : json_null());
    free(sumstr);
    json_t *tail = df2_call(io, "_tail_trunc", a3);
    json_free(a3);
    if (df2_raised(tail)) return tail;
    if (!jtruth(tail)) { json_free(tail); return json_null(); }
    return tail;
}

/* ===================================================================== */
/* PoP: df2_preview_bridge_call @ agent/display.py:_preview_bridge_call    */
/* ===================================================================== */
static json_t *df2_preview_bridge_call(df2_io_t *io, const char *tool_name,
                                       json_t *args, json_t *max_len) {
    json_t *aa = json_object();
    json_set_hdr(aa, "tool_name", tool_name);
    json_set(aa, "args", args ? json_copy(args) : json_null());
    json_t *labels = df2_call(io, "bridge_tool_labels", aa);
    json_free(aa);
    if (df2_raised(labels)) return labels;
    size_t n = j_is_arr(labels) ? json_len(labels) : 0;
    if (n == 0) {
        json_t *pa = json_object();
        json_set_hdr(pa, "tool_name", tool_name);
        json_set(pa, "args", args ? json_copy(args) : json_null());
        json_set(pa, "max_len", max_len ? json_copy(max_len) : json_null());
        json_t *r = df2_call(io, "_primary_arg_preview", pa);
        json_free(pa);
        json_free(labels);
        return r;
    }
    json_t *l0 = json_get(labels, 0);
    const char *text = df2_get_str(l0, "text", "");
    char buf[1024];
    if (n > 1) snprintf(buf, sizeof buf, "%s +%zu", text, n - 1);
    else snprintf(buf, sizeof buf, "%s", text);
    json_free(labels);
    json_t *ta = json_object();
    json_set_hdr(ta, "text", buf);
    json_set(ta, "limit", max_len ? json_copy(max_len) : json_null());
    json_t *tail = df2_call(io, "_tail_trunc", ta);
    json_free(ta);
    if (df2_raised(tail)) return tail;
    if (!jtruth(tail)) { json_free(tail); return json_null(); }
    return tail;
}

/* ===================================================================== */
/* PoP: df2_skin_spinner_list @ agent/display.py:_skin_spinner_list        */
/* ===================================================================== */
static json_t *df2_skin_spinner_list(df2_io_t *io, const char *key, json_t *fallback) {
    json_t *empty = json_object();
    json_t *skin = df2_call(io, "_get_skin", empty);
    json_free(empty);
    if (df2_raised(skin)) {
        if (df2_is_base_exc(skin)) return skin;
        json_free(skin);
        return fallback ? json_copy(fallback) : json_array();
    }
    json_t *val = NULL;
    if (jtruth(skin)) {
        json_t *spinner = df2_get(skin, "spinner");
        if (j_is_obj(spinner)) {
            val = json_obj_get(spinner, key);
        } else if (spinner && !j_is_null(spinner)) {
            json_free(skin);
            return fallback ? json_copy(fallback) : json_array();
        }
    }
    json_t *r = jtruth(val) ? json_copy(val) : (fallback ? json_copy(fallback) : json_array());
    json_free(skin);
    return r;
}

/* ===================================================================== */
/* PoP: df2_catalog_thinking_verbs @ agent/display.py:_catalog_thinking_verbs */
/* ===================================================================== */
static json_t *df2_catalog_thinking_verbs(df2_io_t *io) {
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "display.thinking_verbs");
    json_t *tr = df2_call(io, "t", ta);
    json_free(ta);
    if (df2_raised(tr)) return tr;
    char *ts = df2_pystr(tr);
    json_free(tr);
    json_t *verbs = json_array();
    const char *p = ts;
    while (1) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char *tok = malloc(len + 1);
        memcpy(tok, p, len);
        tok[len] = '\0';
        char *st = df2_strip(tok);
        free(tok);
        if (st && st[0]) json_append(verbs, json_string(st));
        free(st);
        if (!comma) break;
        p = comma + 1;
    }
    free(ts);
    bool use = json_len(verbs) > 0;
    if (use && json_len(verbs) == 1) {
        json_t *v0 = json_get(verbs, 0);
        if (v0 && v0->type == JSON_STRING && strcmp(sval(v0), "display.thinking_verbs") == 0) use = false;
    }
    if (use) return verbs;
    json_free(verbs);
    json_t *out = json_array();
    for (size_t i = 0; DF2_THINKING_VERBS[i]; i++) json_append(out, json_string(DF2_THINKING_VERBS[i]));
    return out;
}

/* ===================================================================== */
/* PoP: df2_is_patch_stdout_proxy @ agent/display.py:_is_patch_stdout_proxy */
/* ===================================================================== */
static json_t *df2_is_patch_stdout_proxy(df2_io_t *io) {
    json_t *empty = json_object();
    json_t *imp = df2_call(io, "StdoutProxy_import", empty);
    json_free(empty);
    if (df2_raised(imp)) {
        if (df2_is_import_error(imp)) { json_free(imp); return json_bool(false); }
        return imp;
    }
    json_free(imp);
    json_t *out = df2_get(io->state, "_out");
    bool is_proxy = j_is_obj(out) && jtruth(json_obj_get(out, "__proxy__"));
    return json_bool(is_proxy);
}

/* ===================================================================== */
/* PoP: df2_start @ agent/display.py:start                                */
/* ===================================================================== */
static json_t *df2_start(df2_io_t *io) {
    if (jtruth(df2_get(io->state, "running"))) return json_null();
    json_t *empty = json_object();
    json_t *tv = df2_call(io, "time.time", empty);
    json_free(empty);
    if (df2_raised(tv)) return tv;
    json_set(io->state, "running", json_bool(true));
    json_set(io->state, "start_time", json_copy(tv));
    json_free(tv);
    json_t *ta = json_object();
    json_set_hdr(ta, "target", "_animate");
    json_set(ta, "daemon", json_bool(true));
    json_t *thr = df2_call(io, "threading.Thread", ta);
    json_free(ta);
    if (df2_raised(thr)) return thr;
    json_set(io->state, "thread", json_copy(thr));
    json_t *sa = json_object();
    json_set(sa, "thread", json_copy(thr));
    json_t *sr = df2_call(io, "thread.start", sa);
    json_free(sa);
    json_free(thr);
    if (df2_raised(sr)) return sr;
    json_free(sr);
    return json_null();
}

/* ===================================================================== */
/* PoP: df2_clear_line_blanks @ agent/display.py:_clear_line_blanks        */
/* ===================================================================== */
static json_t *df2_clear_line_blanks(df2_io_t *io) {
    json_t *v = df2_get(io->state, "last_line_len");
    double lll = (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
    long n = (long)lll + 5;
    if (n < 40) n = 40;
    char *o = malloc((size_t)n + 1);
    if (!o) return json_string("");
    memset(o, ' ', (size_t)n);
    o[n] = '\0';
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* ===================================================================== */
/* dispatch entry (harness calls this)                                    */
/* ===================================================================== */
json_t *df2_dispatch(df2_io_t *io, const char *func, json_t *in) {
    if (strcmp(func, "_preview_shell") == 0)
        return df2_preview_shell(io, json_get_str(in, "key", ""), json_obj_get(in, "args"),
                                 json_obj_get(in, "max_len"));
    if (strcmp(func, "_preview_bridge_call") == 0)
        return df2_preview_bridge_call(io, json_get_str(in, "tool_name", ""),
                                       json_obj_get(in, "args"), json_obj_get(in, "max_len"));
    if (strcmp(func, "_skin_spinner_list") == 0)
        return df2_skin_spinner_list(io, json_get_str(in, "key", ""), json_obj_get(in, "fallback"));
    if (strcmp(func, "_catalog_thinking_verbs") == 0)
        return df2_catalog_thinking_verbs(io);
    if (strcmp(func, "_is_patch_stdout_proxy") == 0)
        return df2_is_patch_stdout_proxy(io);
    if (strcmp(func, "start") == 0)
        return df2_start(io);
    if (strcmp(func, "_clear_line_blanks") == 0)
        return df2_clear_line_blanks(io);
    json_t *e = json_object();
    json_set_hdr(e, "error", "unknown func");
    return e;
}

#endif /* DISPLAY_FLOW2_C */
