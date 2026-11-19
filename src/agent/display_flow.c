/* display_flow.c — FLOW + leaf port of agent/display.py presentation flows.
 *
 * Ports the UNCLAIMED display decision chains: the friendly-label / verb /
 * emoji plumbing, the kawaii completion-row renderers (_cute_*), the bridge
 * label/suffix flows, get_tool_row_parts, and three KawaiiSpinner leaves.
 * Every foreign sub-call (i18n ``t``, skin engine, tool-label registry, JSON
 * loads, logger) and every CLAIMED sibling (_oneline, _tail_trunc, _cute_trunc,
 * _cute_path, build_tool_preview, _display_url, _domain, get_tool_emoji,
 * _get_skin, _detect_tool_failure, _trim_error, redact_tool_args_for_display,
 * _delegate_action_preview, _delegate_task_goals, _browser_exec_step_label,
 * safe_json_loads) is INJECTED through the dp_io_t universal seam:
 * io->call(io, name, args) appends {"name","args"} to io->trace and returns the
 * scripted result (NULL/json_null == None; {"__raise__":cls,"message":msg} == a
 * raised exception). Unclaimed siblings that are themselves ports here
 * (_memory_first_op, tool_labels_for_call, bridge_tool_labels, _cute_row,
 * _cute_fallback_row, _entry_failure_suffix, _bridge_row_suffixes) are seamed
 * when composed and verified for real by their own direct cases.
 *
 * Representation contract (shared with tests/oracle_display_flow.py):
 *  - _CuteRow result -> {"__cute_row__":true,"text","emoji","verb","detail"}.
 *  - tool label double -> {"name","emoji","text","preview"} (dataclasses.replace
 *    models as a copy with emoji overridden).
 *  - _detect_tool_failure seam result -> 2-element array [is_failure, suffix].
 *  - _memory_first_op result -> {"op":<dict>,"more":<str>}.
 *  - get_tool_row_parts result -> list of [emoji, verb, detail, failure].
 *  - Integral floats normalize to ints on both sides; fixtures avoid
 *    integral-float JSON literals and non-finite floats.
 *  - ASCII scope: strip/lstrip/lower/len/slice model ASCII (fixtures ASCII);
 *    the emoji / box-drawing / arrow / ellipsis glyphs below are VERBATIM UTF-8
 *    constants copied from display.py (libjson serializes bytes >= 0x20 as-is).
 *  - Globals (_friendly_tool_labels, _tool_preview_max_len, last_line_len) ride
 *    io->state (read-only here; none of these ports mutates them).
 *
 * Seam name table (name -> live call):
 *   t                          agent.i18n.t(key, **kw)
 *   safe_json_loads            utils.safe_json_loads(s)
 *   _get_skin                  _get_skin()
 *   labels_for_call            tools.tool_labels.labels_for_call(tool_name, args)
 *   logger.debug               logger.debug(fmt, *args, **kw)
 *   tool_labels_for_call       tool_labels_for_call(tool_name, args)
 *   bridge_tool_labels         bridge_tool_labels(tool_name, args)
 *   get_tool_emoji             get_tool_emoji(tool_name, default)
 *   build_tool_preview         build_tool_preview(tool_name, args, max_len)
 *   _cute_trunc                _cute_trunc(s)
 *   _oneline                   _oneline(text)
 *   _domain                    _domain(url)
 *   _display_url               _display_url(value)
 *   _tail_trunc                _tail_trunc(text, limit)
 *   _delegate_action_preview   _delegate_action_preview(args)
 *   _delegate_task_goals       _delegate_task_goals(tasks, per_goal_len)
 *   _detect_tool_failure       _detect_tool_failure(tool_name, result)
 *   _trim_error                _trim_error(msg)
 *   _memory_first_op           _memory_first_op(args)
 *   _browser_exec_step_label   _browser_exec_step_label(args, max_chars)
 *   _entry_failure_suffix      _entry_failure_suffix(entry)
 *   _bridge_row_suffixes       _bridge_row_suffixes(rows, result, call_suffix)
 *   _cute_fallback_row         _cute_fallback_row(tool_name, args)
 *   _cute_row                  _cute_row(emoji, verb, detail)
 *   cute_render                _CUTE_LINES[tool_name](args, result)
 *   redact_tool_args_for_display  redact_tool_args_for_display(tool_name, args)
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. dp_call copies args into the trace; the caller frees args.
 */
#ifndef DISPLAY_FLOW_C
#define DISPLAY_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct dp_io_t {
    json_t *(*call)(struct dp_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} dp_io_t;

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *dp_call(dp_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    return r ? r : json_null();
}

static bool dp_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* BaseException-only trio propagates through `except Exception` sites. */
static bool dp_is_base_exc(const json_t *r) {
    if (!dp_raised(r)) return false;
    json_t *k = json_obj_get(r, "__raise__");
    if (!k || k->type != JSON_STRING) return false;
    const char *s = k->str_val ? k->str_val : "";
    return strcmp(s, "SystemExit") == 0 || strcmp(s, "KeyboardInterrupt") == 0
        || strcmp(s, "GeneratorExit") == 0 || strcmp(s, "CancelledError") == 0;
}

static json_t *dp_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
/* present-non-null (Python attribute presence on a double) */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }

static json_t *dp_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static const char *dp_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = dp_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}
static double dp_get_num(const json_t *obj, const char *key, double def) {
    json_t *v = dp_get(obj, key);
    if (v && v->type == JSON_NUMBER) return v->num_val;
    return def;
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type name of a JSON value (for AttributeError/TypeError messages). */
static const char *dp_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() of a JSON scalar (containers out of fixture scope). */
static char *dp_pystr(const json_t *v) {
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

/* ── ASCII string helpers ──────────────────────────────────────────────── */
static bool dp_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *dp_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && dp_isspace((unsigned char)p[a])) a++;
    while (b > a && dp_isspace((unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}
static char *dp_lstrip_char(const char *s, char ch) {
    const char *p = s ? s : "";
    while (*p == ch) p++;
    return strdup(p);
}
static char *dp_lower(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return o;
}
static bool dp_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}
/* f"{s:9}" — left-justify to min width 9 (ASCII byte length == codepoints). */
static char *dp_ljust9(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    size_t w = n < 9 ? 9 : n;
    char *o = malloc(w + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    for (size_t i = n; i < w; i++) o[i] = ' ';
    o[w] = '\0';
    return o;
}
/* s[:9] (ASCII). */
static char *dp_trunc9(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    if (n > 9) n = 9;
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
/* s[:n] (ASCII), n >= 0. */
static char *dp_truncn(const char *s, size_t n) {
    const char *p = s ? s : "";
    size_t l = strlen(p);
    if (n < l) l = n;
    char *o = malloc(l + 1);
    if (!o) return NULL;
    memcpy(o, p, l);
    o[l] = '\0';
    return o;
}
/* first line: s.split("\n", 1)[0] */
static char *dp_first_line(const char *s) {
    const char *p = s ? s : "";
    const char *nl = strchr(p, '\n');
    size_t n = nl ? (size_t)(nl - p) : strlen(p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
static bool dp_in_set(const char *const *set, const char *val) {
    if (!val) return false;
    for (size_t i = 0; set[i]; i++) if (strcmp(set[i], val) == 0) return true;
    return false;
}

/* len() of a JSON value: array count / string byte length (ASCII); else
 * TypeError "object of type '<type>' has no len()". Returns -1 on raise. */
static long dp_len(const json_t *v, json_t **err) {
    *err = NULL;
    if (j_is_arr(v)) return (long)json_len(v);
    if (j_is_str(v)) return (long)strlen(sval(v));
    char msg[128];
    snprintf(msg, sizeof msg, "object of type '%s' has no len()", dp_type_name(v));
    *err = dp_raise("TypeError", msg);
    return -1;
}

/* v[:n] on a str; present-null / non-str raises TypeError
 * "'<type>' object is not subscriptable". Returns malloc'd str or NULL with
 * *err set. Absent (NULL) is handled by the caller's .get default. */
static char *dp_slice_str(const json_t *v, size_t n, json_t **err) {
    *err = NULL;
    if (j_is_str(v)) return dp_truncn(sval(v), n);
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not subscriptable", dp_type_name(v));
    *err = dp_raise("TypeError", msg);
    return NULL;
}

/* v.strip() on a str; non-str raises AttributeError
 * "'<type>' object has no attribute 'strip'". */
static char *dp_attr_strip(const json_t *v, json_t **err) {
    *err = NULL;
    if (j_is_str(v)) return dp_strip(sval(v));
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", dp_type_name(v));
    *err = dp_raise("AttributeError", msg);
    return NULL;
}

/* ── live module constants (byte-pinned to agent/display.py) ───────────── */
static const char *DP_TOOL_VERBS_NO_PREVIEW[] = { "skills_list", "session_search", NULL };
static const char *DP_TOOL_VERBS_FOR_CONNECTOR[] = { "web_search", "search_files", NULL };
static const char *DP_SCROLL_DIRS[] = { "down", "up", "right", "left", NULL };
static const char *DP_SCROLL_ARROWS[] = { "\xe2\x86\x93", "\xe2\x86\x91", "\xe2\x86\x92", "\xe2\x86\x90", NULL }; /* ↓ ↑ → ← */
static const char *DP_THINKING_VERBS[] = {
    "pondering", "contemplating", "musing", "cogitating", "ruminating", "deliberating",
    "mulling", "reflecting", "processing", "reasoning", "analyzing", "computing",
    "synthesizing", "formulating", "brainstorming", NULL,
};
/* _CUTE_LINES keys (the renderer dispatch table). */
static const char *DP_CUTE_KEYS[] = {
    "web_search", "web_extract", "terminal", "process_manage", "read_file", "write_file",
    "patch", "search_files", "browser_navigate", "browser_snapshot", "browser_click",
    "browser_type", "browser_scroll", "browser_back", "browser_press", "browser_get_images",
    "browser_vision", "todo_list", "session_search", "memory", "skills_list", "skill_view",
    "image_generate", "text_to_speech", "vision_analyze", "send_message", "cronjob_manage",
    "execute_code", "browser_exec", "delegate_task", NULL,
};
static const char *DP_CUTE_PREFIX = "\xe2\x94\x8a ";          /* ┊<space> */
static const char *DP_ARROW = " \xe2\x86\x92 ";               /* <space>→<space> */
static const char *DP_FALLBACK_EMOJI = "\xe2\x9a\xa1";        /* ⚡ */

/* ── _CuteRow / label double helpers ───────────────────────────────────── */
static json_t *dp_cute_row(const char *text, const char *emoji, const char *verb, const char *detail) {
    json_t *o = json_object();
    json_set(o, "__cute_row__", json_bool(true));
    json_set_hdr(o, "text", text ? text : "");
    json_set_hdr(o, "emoji", emoji ? emoji : "");
    json_set_hdr(o, "verb", verb ? verb : "");
    json_set_hdr(o, "detail", detail ? detail : "");
    return o;
}
static bool dp_is_cute_row(const json_t *v) {
    return j_is_obj(v) && json_get_bool(v, "__cute_row__", false);
}
/* Normalize a tool-label double to the full dataclass field set (defaults "").
 * Live labels are ToolLabel dataclass instances; replace() preserves all fields. */
static json_t *dp_label_norm(const json_t *lb) {
    json_t *o = json_object();
    json_set_hdr(o, "name", dp_get_str(lb, "name", ""));
    json_set_hdr(o, "emoji", dp_get_str(lb, "emoji", ""));
    json_set_hdr(o, "text", dp_get_str(lb, "text", ""));
    json_set_hdr(o, "preview", dp_get_str(lb, "preview", ""));
    return o;
}
/* _CuteRow.__new__ strips the emoji. */
static json_t *dp_cute_row_new(const char *text, const char *emoji, const char *verb, const char *detail) {
    char *es = dp_strip(emoji);
    json_t *o = dp_cute_row(text, es ? es : "", verb, detail);
    free(es);
    return o;
}

/* ── regex calls\[(\d+)\] search (hand-ported) ─────────────────────────── */
/* Returns malloc'd digit string of group(1) for the FIRST match, or NULL. */
static char *dp_bridge_call_index(const char *s) {
    if (!s) return NULL;
    const char *p = s;
    while ((p = strstr(p, "calls[")) != NULL) {
        const char *d = p + 6;
        const char *start = d;
        while (*d >= '0' && *d <= '9') d++;
        if (d > start && *d == ']') {
            size_t n = (size_t)(d - start);
            char *o = malloc(n + 1);
            if (!o) return NULL;
            memcpy(o, start, n);
            o[n] = '\0';
            return o;
        }
        p += 6;
    }
    return NULL;
}

/* ── small seam-arg builders ───────────────────────────────────────────── */
static json_t *dp_args_t(const char *key) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    return a;
}

/* ===================================================================== */
/* PoP: dp_memory_first_op @ agent/display.py:_memory_first_op            */
/* ===================================================================== */
static json_t *dp_memory_first_op(dp_io_t *io, json_t *args) {
    json_t *ops = dp_get(args, "operations");
    if (!j_is_arr(ops) || json_len(ops) == 0) {
        json_t *r = json_object();
        json_set(r, "op", args ? json_copy(args) : json_object());
        json_set_hdr(r, "more", "");
        return r;
    }
    json_t *first = json_get(ops, 0);
    json_t *op = j_is_obj(first) ? json_copy(first) : json_object();
    char *more = strdup("");
    if (json_len(ops) > 1) {
        json_t *a = dp_args_t("cli.shared.n_more");
        json_set(a, "count", json_number((double)(json_len(ops) - 1)));
        json_t *tr = dp_call(io, "t", a);
        json_free(a);
        if (dp_raised(tr)) { json_free(op); free(more); return tr; }
        char *ts = dp_pystr(tr);
        json_free(tr);
        size_t n = strlen(ts) + 4;
        more = malloc(n);
        snprintf(more, n, " (%s)", ts);
        free(ts);
    }
    json_t *r = json_object();
    json_set(r, "op", op);
    json_set_hdr(r, "more", more);
    free(more);
    return r;
}

/* ===================================================================== */
/* PoP: dp_tool_labels_for_call @ agent/display.py:tool_labels_for_call   */
/* ===================================================================== */
static json_t *dp_tool_labels_for_call(dp_io_t *io, const char *tool_name, json_t *args) {
    /* try: labels = labels_for_call(tool_name, args or {}) */
    json_t *a = json_object();
    json_set_hdr(a, "tool_name", tool_name);
    json_set(a, "args", jtruth(args) ? json_copy(args) : json_object());
    json_t *labels = dp_call(io, "labels_for_call", a);
    json_free(a);
    if (dp_raised(labels)) {
        if (dp_is_base_exc(labels)) return labels;
        /* except Exception: logger.debug(...); return [] */
        json_t *msg = dp_get(labels, "message");
        char *excs = dp_pystr(msg);
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "bridge labels failed for %s: %s");
        json_t *lar = json_array();
        json_append(lar, json_string(tool_name));
        json_append(lar, json_string(excs));
        json_set(la, "args", lar);
        json_t *lr = dp_call(io, "logger.debug", la);
        json_free(la);
        free(excs);
        json_free(labels);
        if (dp_raised(lr)) return lr;
        json_free(lr);
        return json_array();
    }
    /* skin = _get_skin(); overrides = (skin.tool_emojis if skin and skin.tool_emojis else None) or {} */
    json_t *skin = dp_call(io, "_get_skin", json_object());
    if (dp_raised(skin)) { json_free(labels); return skin; }
    json_t *overrides = NULL;
    if (jtruth(skin)) {
        json_t *te = dp_get(skin, "tool_emojis");
        if (jtruth(te)) overrides = te;
    }
    json_t *out = json_array();
    if (j_is_arr(labels)) {
        for (size_t i = 0; i < json_len(labels); i++) {
            json_t *lb = json_get(labels, i);
            const char *name = dp_get_str(lb, "name", "");
            json_t *ov = overrides ? dp_get(overrides, name) : NULL;
            if (ov) {
                json_t *cp = dp_label_norm(lb);
                json_set(cp, "emoji", json_copy(ov));
                json_append(out, cp);
            } else {
                json_append(out, dp_label_norm(lb));
            }
        }
    }
    json_free(labels);
    json_free(skin);
    return out;
}

/* ===================================================================== */
/* PoP: dp_bridge_tool_labels @ agent/display.py:bridge_tool_labels       */
/* ===================================================================== */
static json_t *dp_bridge_tool_labels(dp_io_t *io, const char *tool_name, json_t *args) {
    bool friendly = json_get_bool(io->state, "_friendly_tool_labels", true);
    if (!friendly) return json_array();
    json_t *a = json_object();
    json_set_hdr(a, "tool_name", tool_name);
    json_set(a, "args", args ? json_copy(args) : json_null());
    json_t *r = dp_call(io, "tool_labels_for_call", a);
    json_free(a);
    if (dp_raised(r)) return r;
    if (j_is_arr(r)) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(r); i++) json_append(out, dp_label_norm(json_get(r, i)));
        json_free(r);
        return out;
    }
    return r;
}

/* ===================================================================== */
/* PoP: dp_tool_row_emoji @ agent/display.py:tool_row_emoji               */
/* ===================================================================== */
static json_t *dp_tool_row_emoji(dp_io_t *io, const char *tool_name, json_t *args, const char *def) {
    json_t *labels = NULL;
    if (jtruth(args)) {
        json_t *a = json_object();
        json_set_hdr(a, "tool_name", tool_name);
        json_set(a, "args", json_copy(args));
        labels = dp_call(io, "bridge_tool_labels", a);
        json_free(a);
        if (dp_raised(labels)) return labels;
    }
    if (j_is_arr(labels) && json_len(labels) > 0) {
        json_t *first = json_get(labels, 0);
        json_t *r = json_string(dp_get_str(first, "emoji", ""));
        json_free(labels);
        return r;
    }
    if (labels) json_free(labels);
    json_t *a = json_object();
    json_set_hdr(a, "tool_name", tool_name);
    json_set_hdr(a, "default", def);
    json_t *r = dp_call(io, "get_tool_emoji", a);
    json_free(a);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_row @ agent/display.py:_cute_row                          */
/* ===================================================================== */
static json_t *dp_cute_row_fn(dp_io_t *io, const char *emoji, const char *verb, const char *detail) {
    char *key = malloc(strlen(verb) + 32);
    sprintf(key, "display.cute.verb.%s", verb);
    json_t *a = dp_args_t(key);
    free(key);
    json_t *label_r = dp_call(io, "t", a);
    json_free(a);
    if (dp_raised(label_r)) return label_r;
    char *label = dp_pystr(label_r);
    json_free(label_r);
    char *pad = dp_ljust9(label);
    size_t n = strlen(DP_CUTE_PREFIX) + strlen(emoji) + 1 + strlen(pad) + 1 + strlen(detail) + 1;
    char *text = malloc(n);
    snprintf(text, n, "%s%s %s %s", DP_CUTE_PREFIX, emoji, pad, detail);
    json_t *r = dp_cute_row_new(text, emoji, label, detail);
    free(text); free(pad); free(label);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_fallback_row @ agent/display.py:_cute_fallback_row        */
/* ===================================================================== */
static json_t *dp_cute_fallback_row(dp_io_t *io, const char *tool_name, json_t *args) {
    /* detail = _cute_trunc(build_tool_preview(tool_name, args) or "") */
    json_t *ba = json_object();
    json_set_hdr(ba, "tool_name", tool_name);
    json_set(ba, "args", args ? json_copy(args) : json_null());
    json_set(ba, "max_len", json_null());
    json_t *pv = dp_call(io, "build_tool_preview", ba);
    json_free(ba);
    if (dp_raised(pv)) return pv;
    json_t *ta = json_object();
    json_set(ta, "s", jtruth(pv) ? json_copy(pv) : json_string(""));
    json_free(pv);
    json_t *detail_r = dp_call(io, "_cute_trunc", ta);
    json_free(ta);
    if (dp_raised(detail_r)) return detail_r;
    char *detail = dp_pystr(detail_r);
    json_free(detail_r);
    /* emoji = get_tool_emoji(tool_name) */
    json_t *ea = json_object();
    json_set_hdr(ea, "tool_name", tool_name);
    json_set_hdr(ea, "default", DP_FALLBACK_EMOJI);
    json_t *emoji_r = dp_call(io, "get_tool_emoji", ea);
    json_free(ea);
    if (dp_raised(emoji_r)) { free(detail); return emoji_r; }
    char *emoji = dp_pystr(emoji_r);
    json_free(emoji_r);
    /* _CuteRow(f"┊ {emoji} {tool_name[:9]:9} {detail}", emoji, tool_name, detail) */
    char *t9 = dp_trunc9(tool_name);
    char *pad = dp_ljust9(t9);
    size_t n = strlen(DP_CUTE_PREFIX) + strlen(emoji) + 1 + strlen(pad) + 1 + strlen(detail) + 1;
    char *text = malloc(n);
    snprintf(text, n, "%s%s %s %s", DP_CUTE_PREFIX, emoji, pad, detail);
    json_t *r = dp_cute_row_new(text, emoji, tool_name, detail);
    free(text); free(pad); free(t9); free(emoji); free(detail);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_web_extract @ agent/display.py:_cute_web_extract          */
/* ===================================================================== */
static json_t *dp_cute_web_extract(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *urls = dp_get(a_args, "urls");
    if (!urls) urls = json_array(); /* a.get("urls", []) — owned temp below */
    bool urls_owned = !dp_get(a_args, "urls");
    char *url = strdup("");
    if (jtruth(urls)) {
        json_t *target = j_is_arr(urls) ? json_get(urls, 0) : urls;
        json_t *da = json_object();
        json_set(da, "value", target ? json_copy(target) : json_null());
        json_t *du = dp_call(io, "_display_url", da);
        json_free(da);
        if (dp_raised(du)) { free(url); if (urls_owned) json_free(urls); return du; }
        free(url);
        url = dp_pystr(du);
        json_free(du);
    }
    if (!url[0]) {
        /* return _cute_row("📄", "fetch", t("display.cute.fetch_pages")) */
        json_t *ta = dp_args_t("display.cute.fetch_pages");
        json_t *tr = dp_call(io, "t", ta);
        json_free(ta);
        if (dp_raised(tr)) { free(url); if (urls_owned) json_free(urls); return tr; }
        char *ts = dp_pystr(tr);
        json_free(tr);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\x93\x84"); /* 📄 */
        json_set_hdr(ca, "verb", "fetch");
        json_set_hdr(ca, "detail", ts);
        json_t *r = dp_call(io, "_cute_row", ca);
        json_free(ca); free(ts); free(url);
        if (urls_owned) json_free(urls);
        return r;
    }
    /* extra = f" +{len(urls)-1}" if isinstance(urls, list) and len(urls) > 1 else "" */
    char *extra = strdup("");
    if (j_is_arr(urls) && json_len(urls) > 1) {
        char buf[32];
        snprintf(buf, sizeof buf, " +%zu", json_len(urls) - 1);
        free(extra); extra = strdup(buf);
    }
    /* _cute_trunc(_domain(url)) */
    json_t *doa = json_object();
    json_set_hdr(doa, "url", url);
    json_t *dom = dp_call(io, "_domain", doa);
    json_free(doa);
    if (dp_raised(dom)) { free(url); free(extra); if (urls_owned) json_free(urls); return dom; }
    json_t *cta = json_object();
    json_set(cta, "s", json_copy(dom));
    json_free(dom);
    json_t *ct = dp_call(io, "_cute_trunc", cta);
    json_free(cta);
    if (dp_raised(ct)) { free(url); free(extra); if (urls_owned) json_free(urls); return ct; }
    char *cts = dp_pystr(ct);
    json_free(ct);
    size_t n = strlen(cts) + strlen(extra) + 1;
    char *detail = malloc(n);
    snprintf(detail, n, "%s%s", cts, extra);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xf0\x9f\x93\x84");
    json_set_hdr(ca, "verb", "fetch");
    json_set_hdr(ca, "detail", detail);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca);
    free(detail); free(cts); free(extra); free(url);
    if (urls_owned) json_free(urls);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_todo_list @ agent/display.py:_cute_todo_list              */
/* ===================================================================== */
static json_t *dp_cute_todo_list(dp_io_t *io, json_t *a_args, json_t *result) {
    json_t *todos_arg = dp_get(a_args, "todos");
    double total = 0, done = 0;
    /* try: summary = (safe_json_loads(result) or {}).get("summary", {}) if result else {} */
    if (jtruth(result)) {
        json_t *sa = json_object();
        json_set(sa, "s", json_copy(result));
        json_t *data = dp_call(io, "safe_json_loads", sa);
        json_free(sa);
        if (dp_raised(data)) {
            if (dp_is_base_exc(data)) return data;
            json_free(data); /* except Exception: pass */
        } else {
            json_t *base = jtruth(data) ? data : NULL;
            if (base && !j_is_obj(base)) {
                /* (data or {}).get on a non-dict truthy value -> AttributeError, caught */
            } else {
                json_t *summary = base ? dp_get(base, "summary") : NULL;
                if (!summary) summary = NULL; /* .get("summary", {}) default {} */
                if (summary && !j_is_obj(summary)) {
                    /* summary.get -> AttributeError, caught */
                } else if (summary) {
                    total = dp_get_num(summary, "total", 0);
                    done = dp_get_num(summary, "completed", 0);
                }
            }
            json_free(data);
        }
    }
    /* progress = t("display.cute.todo_progress", done=done, total=total) */
    json_t *pa = dp_args_t("display.cute.todo_progress");
    json_set(pa, "done", json_number(done));
    json_set(pa, "total", json_number(total));
    json_t *progress_r = dp_call(io, "t", pa);
    json_free(pa);
    if (dp_raised(progress_r)) return progress_r;
    char *progress = dp_pystr(progress_r);
    json_free(progress_r);

    char *detail = NULL;
    bool detail_owned = false;
    if (j_is_null(todos_arg)) {
        if (total > 0) { detail = progress; detail_owned = true; }
        else {
            json_t *ta = dp_args_t("display.cute.todo_reading");
            json_t *tr = dp_call(io, "t", ta);
            json_free(ta);
            if (dp_raised(tr)) { free(progress); return tr; }
            detail = dp_pystr(tr); json_free(tr); detail_owned = true;
        }
    } else {
        bool merge = jtruth(dp_get(a_args, "merge"));
        json_t *lenerr = NULL;
        long cnt = dp_len(todos_arg, &lenerr);
        if (lenerr) { free(progress); return lenerr; }
        if (merge) {
            if (total > 0 && done > 0) {
                json_t *ta = dp_args_t("display.cute.todo_update_progress");
                json_set(ta, "done", json_number(done));
                json_set(ta, "total", json_number(total));
                json_t *tr = dp_call(io, "t", ta);
                json_free(ta);
                if (dp_raised(tr)) { free(progress); return tr; }
                detail = dp_pystr(tr); json_free(tr); detail_owned = true;
            } else {
                json_t *ta = dp_args_t("display.cute.todo_update_count");
                json_set(ta, "count", json_number((double)cnt));
                json_t *tr = dp_call(io, "t", ta);
                json_free(ta);
                if (dp_raised(tr)) { free(progress); return tr; }
                detail = dp_pystr(tr); json_free(tr); detail_owned = true;
            }
        } else {
            if (total > 0 && done > 0) { detail = progress; detail_owned = true; }
            else {
                json_t *ta = dp_args_t("display.cute.todo_count");
                json_set(ta, "count", json_number((double)cnt));
                json_t *tr = dp_call(io, "t", ta);
                json_free(ta);
                if (dp_raised(tr)) { free(progress); return tr; }
                detail = dp_pystr(tr); json_free(tr); detail_owned = true;
            }
        }
    }
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xf0\x9f\x93\x8b"); /* 📋 */
    json_set_hdr(ca, "verb", "plan");
    json_set_hdr(ca, "detail", detail);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca);
    if (detail_owned) free(detail);
    if (!detail_owned) free(progress);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_memory @ agent/display.py:_cute_memory                    */
/* ===================================================================== */
static json_t *dp_cute_memory(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *ma = json_object();
    json_set(ma, "args", a_args ? json_copy(a_args) : json_null());
    json_t *mfo = dp_call(io, "_memory_first_op", ma);
    json_free(ma);
    if (dp_raised(mfo)) return mfo;
    json_t *op = dp_get(mfo, "op");
    const char *more = dp_get_str(mfo, "more", "");
    const char *action = dp_get_str(op, "action", "?");
    const char *target = dp_get_str(a_args, "target", "");
    json_t *ret = NULL;
    if (strcmp(action, "add") == 0) {
        json_t *content = dp_get(op, "content");
        if (!jtruth(content)) content = dp_get(op, "new_text");
        if (!jtruth(content)) content = NULL;
        json_t *cta = json_object();
        json_set(cta, "s", content ? json_copy(content) : json_string(""));
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { json_free(mfo); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        size_t n = strlen(target) + strlen(cts) + strlen(more) + 8;
        char *detail = malloc(n);
        snprintf(detail, n, "+%s: \"%s\"%s", target, cts, more);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\xa7\xa0"); /* 🧠 */
        json_set_hdr(ca, "verb", "memory");
        json_set_hdr(ca, "detail", detail);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(detail); free(cts);
    } else if (strcmp(action, "replace") == 0 || strcmp(action, "patch") == 0 || strcmp(action, "remove") == 0) {
        json_t *old = dp_get(op, "old_text");
        char *olds = NULL;
        if (jtruth(old)) { olds = dp_pystr(old); }
        else {
            json_t *ta = dp_args_t("display.preview.missing_old_text");
            json_t *tr = dp_call(io, "t", ta);
            json_free(ta);
            if (dp_raised(tr)) { json_free(mfo); return tr; }
            olds = dp_pystr(tr); json_free(tr);
        }
        json_t *cta = json_object();
        json_set_hdr(cta, "s", olds);
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { free(olds); json_free(mfo); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        const char *sign = strcmp(action, "remove") == 0 ? "-" : "~";
        size_t n = strlen(target) + strlen(cts) + strlen(more) + 8;
        char *detail = malloc(n);
        snprintf(detail, n, "%s%s: \"%s\"%s", sign, target, cts, more);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\xa7\xa0");
        json_set_hdr(ca, "verb", "memory");
        json_set_hdr(ca, "detail", detail);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(detail); free(cts); free(olds);
    } else {
        char *as = dp_pystr(dp_get(op, "action"));
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\xa7\xa0");
        json_set_hdr(ca, "verb", "memory");
        json_set_hdr(ca, "detail", as);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(as);
    }
    json_free(mfo);
    return ret;
}

/* ===================================================================== */
/* PoP: dp_cute_skill_view @ agent/display.py:_cute_skill_view            */
/* ===================================================================== */
static json_t *dp_cute_skill_view(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    const char *label = dp_get_str(a_args, "name", "");
    json_t *file_path = dp_get(a_args, "file_path");
    char *lab = NULL;
    if (jtruth(file_path)) {
        char *fps = dp_pystr(file_path);
        if (label[0]) {
            size_t n = strlen(label) + strlen(DP_ARROW) + strlen(fps) + 1;
            lab = malloc(n);
            snprintf(lab, n, "%s%s%s", label, DP_ARROW, fps);
        } else {
            lab = strdup(fps);
        }
        free(fps);
    } else {
        lab = strdup(label);
    }
    json_t *cta = json_object();
    json_set_hdr(cta, "s", lab);
    json_t *ct = dp_call(io, "_cute_trunc", cta);
    json_free(cta);
    if (dp_raised(ct)) { free(lab); return ct; }
    char *cts = dp_pystr(ct);
    json_free(ct);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xf0\x9f\x93\x9a"); /* 📚 */
    json_set_hdr(ca, "verb", "skill");
    json_set_hdr(ca, "detail", cts);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(cts); free(lab);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_cronjob @ agent/display.py:_cute_cronjob                  */
/* ===================================================================== */
static json_t *dp_cute_cronjob(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    const char *action = dp_get_str(a_args, "action", "?");
    if (strcmp(action, "create") == 0) {
        /* skills = a.get("skills") or ([a.get("skill")] if a.get("skill") else []) */
        json_t *skills = dp_get(a_args, "skills");
        json_t *skill = dp_get(a_args, "skill");
        /* label = a.get("name") or (skills[0] if skills else None) or a.get("prompt", t(fallback)).
         * The `or` chain SHORT-CIRCUITS: t(fallback) (the .get default) is evaluated
         * ONLY when name and first_skill are both falsy. */
        json_t *name = dp_get(a_args, "name");
        json_t *label_node = NULL;
        json_t *fallback_r = NULL;
        /* effective skills = a.get("skills") or ([skill] if skill else []) */
        json_t *first_skill = NULL;
        if (jtruth(skills) && j_is_arr(skills) && json_len(skills) > 0) first_skill = json_get(skills, 0);
        else if (!jtruth(skills) && jtruth(skill)) first_skill = skill; /* [skill][0] */
        if (jtruth(name)) label_node = name;
        else if (jtruth(first_skill)) label_node = first_skill;
        else {
            json_t *fa = dp_args_t("display.cute.cron_task_fallback");
            fallback_r = dp_call(io, "t", fa);
            json_free(fa);
            if (dp_raised(fallback_r)) return fallback_r;
            json_t *prompt = dp_get(a_args, "prompt");
            label_node = prompt ? prompt : fallback_r;
        }
        /* _cute_trunc(label) */
        json_t *cta = json_object();
        json_set(cta, "s", label_node ? json_copy(label_node) : json_null());
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { if (fallback_r) json_free(fallback_r); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        /* t("display.cute.cron_create", label=_cute_trunc(label)) */
        json_t *ta = dp_args_t("display.cute.cron_create");
        json_set_hdr(ta, "label", cts);
        json_t *tr = dp_call(io, "t", ta);
        json_free(ta);
        if (dp_raised(tr)) { free(cts); if (fallback_r) json_free(fallback_r); return tr; }
        char *ts = dp_pystr(tr);
        json_free(tr);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xe2\x8f\xb0"); /* ⏰ */
        json_set_hdr(ca, "verb", "cron");
        json_set_hdr(ca, "detail", ts);
        json_t *r = dp_call(io, "_cute_row", ca);
        json_free(ca); free(ts); free(cts); if (fallback_r) json_free(fallback_r);
        return r;
    }
    if (strcmp(action, "list") == 0) {
        json_t *ta = dp_args_t("display.cute.cron_listing");
        json_t *tr = dp_call(io, "t", ta);
        json_free(ta);
        if (dp_raised(tr)) return tr;
        char *ts = dp_pystr(tr);
        json_free(tr);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xe2\x8f\xb0");
        json_set_hdr(ca, "verb", "cron");
        json_set_hdr(ca, "detail", ts);
        json_t *r = dp_call(io, "_cute_row", ca);
        json_free(ca); free(ts);
        return r;
    }
    const char *job_id = dp_get_str(a_args, "job_id", "");
    size_t n = strlen(action) + 1 + strlen(job_id) + 1;
    char *detail = malloc(n);
    snprintf(detail, n, "%s %s", action, job_id);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xe2\x8f\xb0");
    json_set_hdr(ca, "verb", "cron");
    json_set_hdr(ca, "detail", detail);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(detail);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_execute_code @ agent/display.py:_cute_execute_code        */
/* ===================================================================== */
static json_t *dp_cute_execute_code(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *code = dp_get(a_args, "code");
    if (!code) code = json_string(""); /* a.get("code", "") */
    bool code_owned = !dp_get(a_args, "code");
    json_t *err = NULL;
    char *stripped = dp_attr_strip(code, &err);
    if (err) { if (code_owned) json_free(code); return err; }
    char *detail_src = strdup("");
    if (stripped[0]) {
        free(detail_src);
        detail_src = dp_first_line(stripped);
    }
    free(stripped);
    json_t *cta = json_object();
    json_set_hdr(cta, "s", detail_src);
    json_t *ct = dp_call(io, "_cute_trunc", cta);
    json_free(cta);
    if (dp_raised(ct)) { free(detail_src); if (code_owned) json_free(code); return ct; }
    char *cts = dp_pystr(ct);
    json_free(ct);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xf0\x9f\x90\x8d"); /* 🐍 */
    json_set_hdr(ca, "verb", "exec");
    json_set_hdr(ca, "detail", cts);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(cts); free(detail_src);
    if (code_owned) json_free(code);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_browser_exec @ agent/display.py:_cute_browser_exec        */
/* ===================================================================== */
static json_t *dp_cute_browser_exec(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *ba = json_object();
    json_set(ba, "args", a_args ? json_copy(a_args) : json_null());
    json_set(ba, "max_chars", json_number(80));
    json_t *label_r = dp_call(io, "_browser_exec_step_label", ba);
    json_free(ba);
    if (dp_raised(label_r)) return label_r;
    char *detail = NULL;
    if (!j_is_null(label_r)) {
        detail = dp_pystr(label_r);
    } else {
        /* _cute_trunc(_oneline(str(a.get("code", "") or ""))) */
        json_t *code = dp_get(a_args, "code");
        char *cs = NULL;
        if (jtruth(code)) cs = dp_pystr(code);
        else cs = strdup("");
        json_t *oa = json_object();
        json_set_hdr(oa, "text", cs);
        json_t *ol = dp_call(io, "_oneline", oa);
        json_free(oa); free(cs);
        if (dp_raised(ol)) { json_free(label_r); return ol; }
        json_t *cta = json_object();
        json_set(cta, "s", json_copy(ol));
        json_free(ol);
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { json_free(label_r); return ct; }
        detail = dp_pystr(ct);
        json_free(ct);
    }
    json_free(label_r);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xf0\x9f\x8c\x90"); /* 🌐 */
    json_set_hdr(ca, "verb", "browser");
    json_set_hdr(ca, "detail", detail);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(detail);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_delegate @ agent/display.py:_cute_delegate                */
/* ===================================================================== */
static json_t *dp_cute_delegate(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *aa = json_object();
    json_set(aa, "args", a_args ? json_copy(a_args) : json_null());
    json_t *ap = dp_call(io, "_delegate_action_preview", aa);
    json_free(aa);
    if (dp_raised(ap)) return ap;
    json_t *tasks = dp_get(a_args, "tasks");
    json_t *ret = NULL;
    if (!j_is_null(ap)) {
        json_t *cta = json_object();
        json_set(cta, "s", json_copy(ap));
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { json_free(ap); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\x94\x80"); /* 🔀 */
        json_set_hdr(ca, "verb", "delegate");
        json_set_hdr(ca, "detail", cts);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(cts);
    } else if (jtruth(tasks) && j_is_arr(tasks)) {
        json_t *ga = json_object();
        json_set(ga, "tasks", json_copy(tasks));
        json_set(ga, "per_goal_len", json_number(30));
        json_t *goals = dp_call(io, "_delegate_task_goals", ga);
        json_free(ga);
        if (dp_raised(goals)) { json_free(ap); return goals; }
        char *joined = NULL;
        if (j_is_arr(goals) && json_len(goals) > 0) {
            /* " | ".join(goals) */
            size_t total = 1;
            for (size_t i = 0; i < json_len(goals); i++) {
                char *gs = dp_pystr(json_get(goals, i));
                total += strlen(gs) + 3;
                free(gs);
            }
            joined = malloc(total);
            joined[0] = '\0';
            for (size_t i = 0; i < json_len(goals); i++) {
                char *gs = dp_pystr(json_get(goals, i));
                if (i) strcat(joined, " | ");
                strcat(joined, gs);
                free(gs);
            }
        } else {
            json_t *ta = dp_args_t("display.cute.delegate_parallel");
            json_t *tr = dp_call(io, "t", ta);
            json_free(ta);
            if (dp_raised(tr)) { json_free(goals); json_free(ap); return tr; }
            joined = dp_pystr(tr);
            json_free(tr);
        }
        json_t *cta = json_object();
        json_set_hdr(cta, "s", joined);
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { free(joined); json_free(goals); json_free(ap); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        long ngoals = (j_is_arr(goals) ? (long)json_len(goals) : 0);
        long ntasks = (long)json_len(tasks);
        long count = ngoals ? ngoals : ntasks;
        size_t n = strlen(cts) + 32;
        char *detail = malloc(n);
        snprintf(detail, n, "%ldx: %s", count, cts);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\x94\x80");
        json_set_hdr(ca, "verb", "delegate");
        json_set_hdr(ca, "detail", detail);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(detail); free(cts); free(joined);
        json_free(goals);
    } else {
        json_t *goal = dp_get(a_args, "goal");
        if (!goal) goal = json_string("");
        bool goal_owned = !dp_get(a_args, "goal");
        json_t *cta = json_object();
        json_set(cta, "s", json_copy(goal));
        json_t *ct = dp_call(io, "_cute_trunc", cta);
        json_free(cta);
        if (dp_raised(ct)) { if (goal_owned) json_free(goal); json_free(ap); return ct; }
        char *cts = dp_pystr(ct);
        json_free(ct);
        json_t *ca = json_object();
        json_set_hdr(ca, "emoji", "\xf0\x9f\x94\x80");
        json_set_hdr(ca, "verb", "delegate");
        json_set_hdr(ca, "detail", cts);
        ret = dp_call(io, "_cute_row", ca);
        json_free(ca); free(cts);
        if (goal_owned) json_free(goal);
    }
    json_free(ap);
    return ret;
}

/* ===================================================================== */
/* PoP: dp_cute_process_manage @ agent/display.py:_cute_process_manage    */
/* ===================================================================== */
static json_t *dp_cute_process_manage(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    const char *action = dp_get_str(a_args, "action", "?");
    json_t *sidv = dp_get(a_args, "session_id");
    char *sid = NULL;
    if (!sidv) sid = strdup("");
    else {
        json_t *err = NULL;
        sid = dp_slice_str(sidv, 12, &err);
        if (err) return err;
    }
    char *detail = NULL;
    if (strcmp(action, "list") == 0) {
        json_t *ta = dp_args_t("display.cute.process_list");
        json_t *tr = dp_call(io, "t", ta);
        json_free(ta);
        if (dp_raised(tr)) { free(sid); return tr; }
        detail = dp_pystr(tr);
        json_free(tr);
    } else {
        size_t n = strlen(action) + 1 + strlen(sid) + 1;
        detail = malloc(n);
        snprintf(detail, n, "%s %s", action, sid);
    }
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", "\xe2\x9a\x99\xef\xb8\x8f "); /* ⚙️<space> */
    json_set_hdr(ca, "verb", "proc");
    json_set_hdr(ca, "detail", detail);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(detail); free(sid);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_scroll @ agent/display.py:_cute_scroll                    */
/* ===================================================================== */
static json_t *dp_cute_scroll(dp_io_t *io, json_t *a_args, json_t *result) {
    (void)result;
    json_t *dirv = dp_get(a_args, "direction");
    const char *direction = j_is_str(dirv) ? sval(dirv) : "down";
    bool known = dp_in_set(DP_SCROLL_DIRS, direction);
    char *label = NULL;
    if (known) {
        char *key = malloc(strlen(direction) + 32);
        sprintf(key, "display.cute.scroll.%s", direction);
        json_t *ta = dp_args_t(key);
        free(key);
        json_t *tr = dp_call(io, "t", ta);
        json_free(ta);
        if (dp_raised(tr)) return tr;
        label = dp_pystr(tr);
        json_free(tr);
    } else {
        label = dp_pystr(dirv);
    }
    const char *arrow = DP_SCROLL_ARROWS[0]; /* default ↓ */
    for (size_t i = 0; DP_SCROLL_DIRS[i]; i++)
        if (strcmp(DP_SCROLL_DIRS[i], direction) == 0) { arrow = DP_SCROLL_ARROWS[i]; break; }
    size_t en = strlen(arrow) + 2;
    char *emoji = malloc(en);
    snprintf(emoji, en, "%s ", arrow);
    json_t *ca = json_object();
    json_set_hdr(ca, "emoji", emoji);
    json_set_hdr(ca, "verb", "scroll");
    json_set_hdr(ca, "detail", label);
    json_t *r = dp_call(io, "_cute_row", ca);
    json_free(ca); free(emoji); free(label);
    return r;
}

/* ===================================================================== */
/* PoP: dp_cute_bridge_rows @ agent/display.py:_cute_bridge_rows          */
/* ===================================================================== */
static json_t *dp_cute_bridge_rows(dp_io_t *io, const char *tool_name, json_t *args) {
    json_t *ba = json_object();
    json_set_hdr(ba, "tool_name", tool_name);
    json_set(ba, "args", args ? json_copy(args) : json_null());
    json_t *labels = dp_call(io, "bridge_tool_labels", ba);
    json_free(ba);
    if (dp_raised(labels)) return labels;
    if (!j_is_arr(labels) || json_len(labels) == 0) {
        json_free(labels);
        return json_null();
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(labels); i++) {
        json_t *lb = json_get(labels, i);
        const char *emoji = dp_get_str(lb, "emoji", "");
        /* _cute_trunc(label.text) */
        json_t *ta = json_object();
        json_set_hdr(ta, "s", dp_get_str(lb, "text", ""));
        json_t *tt = dp_call(io, "_cute_trunc", ta);
        json_free(ta);
        if (dp_raised(tt)) { json_free(out); json_free(labels); return tt; }
        char *tts = dp_pystr(tt);
        json_free(tt);
        char *preview_part = strdup("");
        json_t *pv = dp_get(lb, "preview");
        if (jtruth(pv)) {
            json_t *pa = json_object();
            json_set(pa, "s", json_copy(pv));
            json_t *pt = dp_call(io, "_cute_trunc", pa);
            json_free(pa);
            if (dp_raised(pt)) { free(tts); free(preview_part); json_free(out); json_free(labels); return pt; }
            char *pts = dp_pystr(pt);
            json_free(pt);
            size_t pn = strlen(pts) + 4;
            preview_part = malloc(pn);
            snprintf(preview_part, pn, "  %s", pts);
            free(pts);
        }
        size_t n = strlen(DP_CUTE_PREFIX) + strlen(emoji) + 1 + strlen(tts) + strlen(preview_part) + 1;
        char *row = malloc(n);
        snprintf(row, n, "%s%s %s%s", DP_CUTE_PREFIX, emoji, tts, preview_part);
        json_append(out, json_string(row));
        free(row); free(tts); free(preview_part);
    }
    json_free(labels);
    return out;
}

/* ===================================================================== */
/* PoP: dp_entry_failure_suffix @ agent/display.py:_entry_failure_suffix  */
/* ===================================================================== */
static json_t *dp_entry_failure_suffix(dp_io_t *io, json_t *entry) {
    if (!j_is_obj(entry)) return json_string("");
    json_t *error = dp_get(entry, "error");
    if (j_is_obj(error)) {
        /* str(error.get('message') or error.get('code') or t('display.failure.generic_word')) */
        json_t *msg = dp_get(error, "message");
        json_t *code = dp_get(error, "code");
        json_t *chosen = NULL;
        char *chosen_s = NULL;
        if (jtruth(msg)) chosen = msg;
        else if (jtruth(code)) chosen = code;
        if (chosen) chosen_s = dp_pystr(chosen);
        else {
            json_t *ta = dp_args_t("display.failure.generic_word");
            json_t *tr = dp_call(io, "t", ta);
            json_free(ta);
            if (dp_raised(tr)) return tr;
            chosen_s = dp_pystr(tr);
            json_free(tr);
        }
        json_t *tra = json_object();
        json_set_hdr(tra, "msg", chosen_s);
        json_t *trimmed = dp_call(io, "_trim_error", tra);
        json_free(tra); free(chosen_s);
        if (dp_raised(trimmed)) return trimmed;
        char *ts = dp_pystr(trimmed);
        json_free(trimmed);
        size_t n = strlen(ts) + 4;
        char *out = malloc(n);
        snprintf(out, n, " [%s]", ts);
        json_t *r = json_string(out);
        free(out); free(ts);
        return r;
    }
    json_t *success = dp_get(entry, "success");
    bool success_false = success && success->type == JSON_BOOL && !success->bool_val;
    if (!(jtruth(error) || success_false)) return json_string("");
    /* _detect_tool_failure(str(entry.get("name") or ""), entry)[1] */
    json_t *name = dp_get(entry, "name");
    char *names = jtruth(name) ? dp_pystr(name) : strdup("");
    json_t *da = json_object();
    json_set_hdr(da, "tool_name", names);
    json_set(da, "result", json_copy(entry));
    json_t *det = dp_call(io, "_detect_tool_failure", da);
    json_free(da); free(names);
    if (dp_raised(det)) return det;
    json_t *suffix = j_is_arr(det) ? json_get(det, 1) : NULL;
    json_t *r = json_string(suffix && suffix->type == JSON_STRING ? sval(suffix) : "");
    json_free(det);
    return r;
}

/* ===================================================================== */
/* PoP: dp_bridge_row_suffixes @ agent/display.py:_bridge_row_suffixes    */
/* ===================================================================== */
static json_t *dp_bridge_row_suffixes(dp_io_t *io, long rows, json_t *result, const char *call_suffix) {
    json_t *suffixes = json_array();
    for (long i = 0; i < rows; i++) json_append(suffixes, json_string(""));
    /* data = result if isinstance(result, dict) else safe_json_loads(result) */
    json_t *data = NULL;
    bool data_owned = false;
    if (j_is_obj(result)) { data = result; }
    else {
        json_t *sa = json_object();
        json_set(sa, "s", result ? json_copy(result) : json_null());
        data = dp_call(io, "safe_json_loads", sa);
        json_free(sa);
        data_owned = true;
        if (dp_raised(data)) { json_free(suffixes); return data; }
    }
    json_t *entries = j_is_obj(data) ? dp_get(data, "results") : NULL;
    if (j_is_arr(entries)) {
        long lim = (long)json_len(entries);
        if (lim > rows) lim = rows;
        for (long position = 0; position < lim; position++) {
            json_t *entry = json_get(entries, (size_t)position);
            json_t *ea = json_object();
            json_set(ea, "entry", entry ? json_copy(entry) : json_null());
            json_t *suffix_r = dp_call(io, "_entry_failure_suffix", ea);
            json_free(ea);
            if (dp_raised(suffix_r)) { json_free(suffixes); if (data_owned) json_free(data); return suffix_r; }
            const char *suffix = sval(suffix_r);
            bool has = suffix[0] != '\0';
            json_free(suffix_r);
            if (!has) continue;
            json_t *index = j_is_obj(entry) ? dp_get(entry, "index") : NULL;
            long target = position;
            if (index && index->type == JSON_NUMBER) {
                double d = index->num_val;
                if (d == floor(d) && d >= 0 && d < (double)rows) target = (long)d;
            } else if (index && index->type == JSON_BOOL) {
                /* isinstance(bool, int) is True; True==1, False==0 */
                long d = index->bool_val ? 1 : 0;
                if (d >= 0 && d < rows) target = d;
            }
            json_array_set(suffixes, (size_t)target, json_string(suffix));
        }
    }
    /* if call_suffix and not any(suffixes): regex calls[N] */
    bool any = false;
    for (long i = 0; i < rows; i++) {
        json_t *s = json_get(suffixes, (size_t)i);
        if (s && s->type == JSON_STRING && sval(s)[0]) { any = true; break; }
    }
    if (call_suffix && call_suffix[0] && !any) {
        char *digits = dp_bridge_call_index(call_suffix);
        long named = rows - 1;
        if (digits) { named = strtol(digits, NULL, 10); free(digits); }
        long target = (named >= 0 && named < rows) ? named : rows - 1;
        if (target >= 0 && target < rows) {
            json_array_set(suffixes, (size_t)target, json_string(call_suffix));
        }
    }
    if (data_owned) json_free(data);
    return suffixes;
}

/* ===================================================================== */
/* PoP: dp_get_tool_row_parts @ agent/display.py:get_tool_row_parts       */
/* ===================================================================== */
static json_t *dp_get_tool_row_parts(dp_io_t *io, const char *tool_name, json_t *args,
                                     json_t *result, bool is_error) {
    /* try: ... except Exception: logger.debug + fallback row */
    json_t *ret = NULL;
    /* args = redact_tool_args_for_display(tool_name, args) or args or {} */
    json_t *ra = json_object();
    json_set_hdr(ra, "tool_name", tool_name);
    json_set(ra, "args", args ? json_copy(args) : json_null());
    json_t *redacted = dp_call(io, "redact_tool_args_for_display", ra);
    json_free(ra);
    if (dp_raised(redacted)) {
        if (dp_is_base_exc(redacted)) return redacted;
        json_free(redacted);
        goto except_block;
    }
    json_t *eff_args = NULL;
    bool eff_owned = false;
    if (jtruth(redacted)) { eff_args = redacted; eff_owned = true; }
    else {
        /* redacted not adopted: free it here; eff_args owns its own value */
        json_free(redacted);
        if (jtruth(args)) { eff_args = args; }
        else { eff_args = json_object(); eff_owned = true; }
    }

    /* is_failure, suffix = _detect_tool_failure(tool_name, result) */
    json_t *da = json_object();
    json_set_hdr(da, "tool_name", tool_name);
    json_set(da, "result", result ? json_copy(result) : json_null());
    json_t *det = dp_call(io, "_detect_tool_failure", da);
    json_free(da);
    if (dp_raised(det)) {
        if (dp_is_base_exc(det)) { if (eff_owned) json_free(eff_args); return det; }
        json_free(det);
        if (eff_owned) json_free(eff_args);
        goto except_block;
    }
    bool is_failure = false;
    char *suffix = strdup("");
    if (j_is_arr(det) && json_len(det) >= 2) {
        json_t *f = json_get(det, 0);
        is_failure = jtruth(f);
        json_t *s = json_get(det, 1);
        free(suffix);
        suffix = strdup(s && s->type == JSON_STRING ? sval(s) : "");
    }
    json_free(det);
    /* call_failure = suffix if is_failure else ("" if is_error else None) */
    json_t *call_failure = NULL; /* NULL == None */
    if (is_failure) call_failure = json_string(suffix);
    else if (is_error) call_failure = json_string("");

    /* labels = bridge_tool_labels(tool_name, args) */
    json_t *ba = json_object();
    json_set_hdr(ba, "tool_name", tool_name);
    json_set(ba, "args", json_copy(eff_args));
    json_t *labels = dp_call(io, "bridge_tool_labels", ba);
    json_free(ba);
    if (dp_raised(labels)) {
        if (dp_is_base_exc(labels)) { free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return labels; }
        json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
        goto except_block;
    }
    if (j_is_arr(labels) && json_len(labels) > 0) {
        long nlabels = (long)json_len(labels);
        json_t *sa = json_object();
        json_set(sa, "rows", json_number((double)nlabels));
        json_set(sa, "result", result ? json_copy(result) : json_null());
        json_set_hdr(sa, "call_suffix", is_failure ? suffix : "");
        json_t *suffixes = dp_call(io, "_bridge_row_suffixes", sa);
        json_free(sa);
        if (dp_raised(suffixes)) {
            if (dp_is_base_exc(suffixes)) { json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return suffixes; }
            json_free(suffixes); json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
            goto except_block;
        }
        /* marks = [s or None for s in suffixes]; if call_failure is not None and not any(suffixes): marks[-1] = call_failure */
        bool any = false;
        long nsuf = j_is_arr(suffixes) ? (long)json_len(suffixes) : 0;
        for (long i = 0; i < nsuf; i++) {
            json_t *s = json_get(suffixes, (size_t)i);
            if (s && s->type == JSON_STRING && sval(s)[0]) { any = true; break; }
        }
        json_t *out = json_array();
        for (long i = 0; i < nlabels; i++) {
            json_t *lb = json_get(labels, (size_t)i);
            json_t *mark = NULL;
            if (i < nsuf) {
                json_t *s = json_get(suffixes, (size_t)i);
                if (s && s->type == JSON_STRING && sval(s)[0]) mark = json_string(sval(s));
            }
            if (i == nlabels - 1 && call_failure && !any) {
                if (mark) json_free(mark);
                mark = json_copy(call_failure);
            }
            /* _cute_trunc(lb.text) */
            json_t *ta = json_object();
            json_set_hdr(ta, "s", dp_get_str(lb, "text", ""));
            json_t *tt = dp_call(io, "_cute_trunc", ta);
            json_free(ta);
            if (dp_raised(tt)) {
                if (dp_is_base_exc(tt)) { json_free(out); json_free(suffixes); json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return tt; }
                json_free(tt); json_free(out); json_free(suffixes); json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
                goto except_block;
            }
            char *text_s = dp_pystr(tt);
            json_free(tt);
            char *preview_s = strdup("");
            json_t *pv = dp_get(lb, "preview");
            if (jtruth(pv)) {
                json_t *pa = json_object();
                json_set(pa, "s", json_copy(pv));
                json_t *pt = dp_call(io, "_cute_trunc", pa);
                json_free(pa);
                if (dp_raised(pt)) {
                    if (dp_is_base_exc(pt)) { free(text_s); free(preview_s); json_free(out); json_free(suffixes); json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return pt; }
                    json_free(pt); free(text_s); free(preview_s); json_free(out); json_free(suffixes); json_free(labels); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
                    goto except_block;
                }
                free(preview_s);
                preview_s = dp_pystr(pt);
                json_free(pt);
            }
            json_t *row = json_array();
            json_append(row, json_string(dp_get_str(lb, "emoji", "")));
            json_append(row, json_string(text_s));
            json_append(row, json_string(preview_s));
            json_append(row, mark ? mark : json_null());
            json_append(out, row);
            free(text_s); free(preview_s);
        }
        json_free(suffixes);
        json_free(labels);
        free(suffix);
        if (call_failure) json_free(call_failure);
        if (eff_owned) json_free(eff_args);
        return out;
    }
    json_free(labels);
    /* render = _CUTE_LINES.get(tool_name); row = render(args, result) if render else None */
    json_t *row = NULL;
    bool row_owned = false;
    if (dp_in_set(DP_CUTE_KEYS, tool_name)) {
        json_t *ca = json_object();
        json_set_hdr(ca, "tool_name", tool_name);
        json_set(ca, "args", json_copy(eff_args));
        json_set(ca, "result", result ? json_copy(result) : json_null());
        row = dp_call(io, "cute_render", ca);
        json_free(ca);
        row_owned = true;
        if (dp_raised(row)) {
            if (dp_is_base_exc(row)) { free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return row; }
            json_free(row); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
            goto except_block;
        }
    }
    /* row = row if isinstance(row, _CuteRow) else _cute_fallback_row(tool_name, args) */
    if (!dp_is_cute_row(row)) {
        if (row_owned) json_free(row);
        json_t *fa = json_object();
        json_set_hdr(fa, "tool_name", tool_name);
        json_set(fa, "args", json_copy(eff_args));
        row = dp_call(io, "_cute_fallback_row", fa);
        json_free(fa);
        row_owned = true;
        if (dp_raised(row)) {
            if (dp_is_base_exc(row)) { free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args); return row; }
            json_free(row); free(suffix); if (call_failure) json_free(call_failure); if (eff_owned) json_free(eff_args);
            goto except_block;
        }
    }
    json_t *out = json_array();
    json_t *r0 = json_array();
    json_append(r0, json_string(dp_get_str(row, "emoji", "")));
    json_append(r0, json_string(dp_get_str(row, "verb", "")));
    json_append(r0, json_string(dp_get_str(row, "detail", "")));
    json_append(r0, call_failure ? json_copy(call_failure) : json_null());
    json_append(out, r0);
    if (row_owned) json_free(row);
    free(suffix);
    if (call_failure) json_free(call_failure);
    if (eff_owned) json_free(eff_args);
    return out;

except_block:
    /* logger.debug("Tool row parts failed for %s", tool_name, exc_info=True) */
    {
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "Tool row parts failed for %s");
        json_t *lar = json_array();
        json_append(lar, json_string(tool_name));
        json_set(la, "args", lar);
        json_set(la, "exc_info", json_bool(true));
        json_t *lr = dp_call(io, "logger.debug", la);
        json_free(la);
        if (dp_raised(lr)) return lr;
        json_free(lr);
    }
    ret = json_array();
    {
        json_t *r0 = json_array();
        json_append(r0, json_string(DP_FALLBACK_EMOJI));
        json_append(r0, json_string(tool_name));
        json_append(r0, json_string(""));
        json_append(r0, json_null());
        json_append(ret, r0);
    }
    return ret;
}

/* ===================================================================== */
/* PoP: dp_skin_spinner_list @ agent/display.py:KawaiiSpinner._skin_spinner_list */
/* ===================================================================== */
static json_t *dp_skin_spinner_list(dp_io_t *io, const char *key, json_t *fallback) {
    /* try: skin = _get_skin(); return (skin.spinner.get(key, []) if skin else None) or fallback */
    json_t *skin = dp_call(io, "_get_skin", json_object());
    if (dp_raised(skin)) {
        if (dp_is_base_exc(skin)) return skin;
        json_free(skin);
        return fallback ? json_copy(fallback) : json_array();
    }
    json_t *val = NULL;
    if (jtruth(skin)) {
        json_t *spinner = dp_get(skin, "spinner");
        /* skin.spinner.get(key, []) — spinner must be a dict; non-dict -> AttributeError caught */
        if (j_is_obj(spinner)) {
            val = dp_get(spinner, key);
        } else if (spinner && !j_is_null(spinner)) {
            /* AttributeError on .get — caught by except Exception */
            json_free(skin);
            return fallback ? json_copy(fallback) : json_array();
        }
    }
    json_t *r = jtruth(val) ? json_copy(val) : (fallback ? json_copy(fallback) : json_array());
    json_free(skin);
    return r;
}

/* ===================================================================== */
/* PoP: dp_catalog_thinking_verbs @ agent/display.py:KawaiiSpinner._catalog_thinking_verbs */
/* ===================================================================== */
static json_t *dp_catalog_thinking_verbs(dp_io_t *io) {
    json_t *ta = dp_args_t("display.thinking_verbs");
    json_t *tr = dp_call(io, "t", ta);
    json_free(ta);
    if (dp_raised(tr)) return tr;
    char *ts = dp_pystr(tr);
    json_free(tr);
    /* verbs = [v.strip() for v in ts.split(",") if v.strip()] */
    json_t *verbs = json_array();
    {
        const char *p = ts;
        while (1) {
            const char *comma = strchr(p, ',');
            size_t len = comma ? (size_t)(comma - p) : strlen(p);
            char *tok = malloc(len + 1);
            memcpy(tok, p, len);
            tok[len] = '\0';
            char *st = dp_strip(tok);
            free(tok);
            if (st[0]) json_append(verbs, json_string(st));
            free(st);
            if (!comma) break;
            p = comma + 1;
        }
    }
    free(ts);
    /* return verbs if verbs and verbs != ["display.thinking_verbs"] else THINKING_VERBS */
    bool use = json_len(verbs) > 0;
    if (use && json_len(verbs) == 1) {
        json_t *v0 = json_get(verbs, 0);
        if (v0 && v0->type == JSON_STRING && strcmp(sval(v0), "display.thinking_verbs") == 0) use = false;
    }
    if (use) return verbs;
    json_free(verbs);
    json_t *out = json_array();
    for (size_t i = 0; DP_THINKING_VERBS[i]; i++) json_append(out, json_string(DP_THINKING_VERBS[i]));
    return out;
}

/* ===================================================================== */
/* PoP: dp_clear_line_blanks @ agent/display.py:KawaiiSpinner._clear_line_blanks */
/* ===================================================================== */
static json_t *dp_clear_line_blanks(dp_io_t *io) {
    double lll = dp_get_num(io->state, "last_line_len", 0);
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
json_t *dp_dispatch(dp_io_t *io, const char *func, json_t *in) {
    if (strcmp(func, "_memory_first_op") == 0)
        return dp_memory_first_op(io, json_obj_get(in, "args"));
    if (strcmp(func, "tool_labels_for_call") == 0)
        return dp_tool_labels_for_call(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"));
    if (strcmp(func, "bridge_tool_labels") == 0)
        return dp_bridge_tool_labels(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"));
    if (strcmp(func, "tool_row_emoji") == 0)
        return dp_tool_row_emoji(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"),
                                 json_get_str(in, "default", DP_FALLBACK_EMOJI));
    if (strcmp(func, "_cute_row") == 0)
        return dp_cute_row_fn(io, json_get_str(in, "emoji", ""), json_get_str(in, "verb", ""),
                              json_get_str(in, "detail", ""));
    if (strcmp(func, "_cute_fallback_row") == 0)
        return dp_cute_fallback_row(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"));
    if (strcmp(func, "_cute_web_extract") == 0)
        return dp_cute_web_extract(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_todo_list") == 0)
        return dp_cute_todo_list(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_memory") == 0)
        return dp_cute_memory(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_skill_view") == 0)
        return dp_cute_skill_view(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_cronjob") == 0)
        return dp_cute_cronjob(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_execute_code") == 0)
        return dp_cute_execute_code(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_browser_exec") == 0)
        return dp_cute_browser_exec(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_delegate") == 0)
        return dp_cute_delegate(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_process_manage") == 0)
        return dp_cute_process_manage(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_scroll") == 0)
        return dp_cute_scroll(io, json_obj_get(in, "args"), json_obj_get(in, "result"));
    if (strcmp(func, "_cute_bridge_rows") == 0)
        return dp_cute_bridge_rows(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"));
    if (strcmp(func, "_entry_failure_suffix") == 0)
        return dp_entry_failure_suffix(io, json_obj_get(in, "entry"));
    if (strcmp(func, "_bridge_row_suffixes") == 0)
        return dp_bridge_row_suffixes(io, (long)json_get_num(in, "rows", 0), json_obj_get(in, "result"),
                                      json_get_str(in, "call_suffix", ""));
    if (strcmp(func, "get_tool_row_parts") == 0)
        return dp_get_tool_row_parts(io, json_get_str(in, "tool_name", ""), json_obj_get(in, "args"),
                                     json_obj_get(in, "result"), json_get_bool(in, "is_error", false));
    if (strcmp(func, "_skin_spinner_list") == 0)
        return dp_skin_spinner_list(io, json_get_str(in, "key", ""), json_obj_get(in, "fallback"));
    if (strcmp(func, "_catalog_thinking_verbs") == 0)
        return dp_catalog_thinking_verbs(io);
    if (strcmp(func, "_clear_line_blanks") == 0)
        return dp_clear_line_blanks(io);
    json_t *e = json_object();
    json_set_hdr(e, "error", "unknown func");
    return e;
}

#endif /* DISPLAY_FLOW_C */