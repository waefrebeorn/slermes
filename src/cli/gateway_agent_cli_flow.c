/*
 * gateway_agent_cli_flow.c — gateway agent-bridge FLOW cores + leaves of
 * hermes_cli/gateway_agent.py (LIVE authority: GatewayAgent + module helpers).
 *
 * Self-contained C11, libjson-only. Ports the DECISION sequences of the
 * gateway-backed AIAgent stand-in that the sibling port_gateway_agent_flow.c
 * does NOT claim: usage/stats application, session-figure reset, the interrupt
 * / steer / redirect controls, content-part staging, the lost-turn notice, the
 * failed/unadmitted result builders, the print/review-summary emit chain, turn
 * ownership, the rate-limit-state rebuild, the MoA progress handler and the
 * review-summary / api-error leaves.
 *
 * SEAM DOCTRINE: every live effect (connection.call, time.monotonic, the
 * claimed siblings _correct / _give_back_turn_route, the seamed collaborators
 * _reset_session_figures / _refresh_stats / apply_session_usage /
 * _anchor_display / _rate_limit_state, pending_lines, print / print_fn, the
 * MoA tool_progress_callback and the RateLimitBucket / RateLimitState ctors)
 * enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; the caller frees args); the result is
 * a NEW json_t* (NULL == None; a JSON null result is delivered as a NULL
 * pointer; {"__raise__": cls, "msg": ...} == a raised exception). The harness
 * records {"ev": name, "args": args, "ret": result} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python with
 * the same scripts. The cores perform NO I/O.
 *
 * Exception propagation: an UNCAUGHT seam raise parks its spec in the
 * file-static gac_pending channel and the core returns a sentinel; the harness
 * emits the parked spec as ret. `except Exception` sites swallow plain
 * Exception specs but pass the BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) through. Fail-closed: a missing/empty
 * seam script yields a RuntimeError raise spec (recorded, then propagated or
 * swallowed exactly as live).
 *
 * The agent self-model is an injected json_t* `st` mirroring instance
 * attributes; st["state"] is the shared session-state object (info, messages,
 * usage, execution_generation, pending). Mutations happen in place.
 * Ownership: returned json_t* / char* are malloc'd (caller frees) unless noted.
 *
 * Fixture domain (ASCII): context_length / captured_at are numeric or null
 * (int()/float() string-parse raise messages are out of scope); splitlines
 * inputs use \n \r \r\n \v \f \x1c-\x1e; integral floats normalize to ints on
 * both sides via the C-serialize round-trip.
 *
 * Seam name table (name -> live call):
 *   call                 self.connection.call(method, **kwargs)  args{method,...}
 *   monotonic            time.monotonic() -> number
 *   _correct             self._correct(verb, text)  [claimed sibling]
 *   _give_back_turn_route self._give_back_turn_route()  [claimed sibling]
 *   _reset_session_figures self._reset_session_figures()
 *   _refresh_stats       self._refresh_stats() -> bool
 *   apply_session_usage  self.apply_session_usage(usage)
 *   _anchor_display      self._anchor_display()
 *   _rate_limit_state    module _rate_limit_state(data)
 *   pending_lines        gateway_chat_view.pending_lines(pending) -> [str]
 *   print                print(text)  (via _emit_print, no _print_fn)
 *   print_fn             self._print_fn(text)  (via _emit_print)
 *   tool_progress_callback agent.tool_progress_callback(kind,label,text,None,**extra)
 *   RateLimitBucket      RateLimitBucket(**kwargs)
 *   RateLimitState       RateLimitState(**buckets, captured_at=, provider=)
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── universal seam plumbing ── */
typedef struct gac_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gac_seams_t;

static json_t *gac_pending = NULL;  /* parked uncaught raise spec */

static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
/* isinstance(v, (int, float)) — bool is an int subclass in Python. */
static bool gac_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
/* isinstance(v, int) — integral number OR bool (after the int-roundtrip). */
static bool gac_is_int(const json_t *v) {
    if (!v) return false;
    if (v->type == JSON_BOOL) return true;
    if (v->type == JSON_NUMBER) return v->num_val == trunc(v->num_val);
    return false;
}
static double gac_num(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static char *xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

static json_t *gac_call(const gac_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *gac_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
static bool gac_is_base(const char *cls) {
    return cls && (!strcmp(cls, "SystemExit") || !strcmp(cls, "KeyboardInterrupt")
                   || !strcmp(cls, "GeneratorExit"));
}
/* Park an uncaught raise spec (takes ownership) and signal propagation. */
static void gac_park(json_t *r) { gac_pending = r; }
static json_t *gac_take_pending(void) { json_t *r = gac_pending; gac_pending = NULL; return r; }

static json_t *args0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* st["state"] borrowed (or NULL). */
static json_t *self_state(json_t *st) { return json_obj_get(st, "state"); }
static const json_t *state_info(json_t *st) {
    json_t *state = self_state(st);
    const json_t *info = state ? json_obj_get(state, "info") : NULL;
    return is_obj(info) ? info : NULL;
}

/* Python str.strip() over the ASCII White_Space set. */
static char *strip_ws(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r' || *b == '\v' || *b == '\f')) b++;
    const char *e = s + strlen(s);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\v' || e[-1] == '\f')) e--;
    size_t n = (size_t)(e - b);
    char *out = malloc(n + 1);
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* _RESULT_DEFAULTS (module constant) as a fresh object. */
static json_t *result_defaults(void) {
    json_t *d = json_object();
    json_set(d, "final_response", json_string(""));
    json_set(d, "failed", json_bool(false));
    json_set(d, "partial", json_bool(false));
    json_set(d, "error", json_null());
    json_set(d, "failure_reason", json_null());
    json_set(d, "completed", json_bool(true));
    json_set(d, "interrupted", json_bool(false));
    json_set(d, "interrupt_message", json_null());
    json_set(d, "api_calls", json_number(0));
    json_set(d, "pending_steer", json_null());
    json_set(d, "last_reasoning", json_null());
    json_set(d, "response_previewed", json_bool(false));
    json_set(d, "response_transformed", json_bool(false));
    json_set(d, "pre_transform_response", json_null());
    json_set(d, "billing_block", json_null());
    json_set(d, "auxiliary_usage", json_object());
    return d;
}

/* _COUNTER_FROM_USAGE: session attr -> usage key, in live insertion order. */
static const char *COUNTER_PAIRS[][2] = {
    {"session_input_tokens", "input_tokens"},
    {"session_output_tokens", "output_tokens"},
    {"session_prompt_tokens", "prompt_tokens"},
    {"session_completion_tokens", "completion_tokens"},
    {"session_total_tokens", "total_tokens"},
    {"session_cache_read_tokens", "cache_read_tokens"},
    {"session_cache_write_tokens", "cache_write_tokens"},
    {"session_reasoning_tokens", "reasoning_tokens"},
    {"session_api_calls", "api_calls"},
};
#define COUNTER_N (sizeof(COUNTER_PAIRS) / sizeof(COUNTER_PAIRS[0]))

/* ── review_summary_line (module leaf) ── */
/* PoP: gac_review_summary_line @ hermes_cli/gateway_agent.py:review_summary_line */
static char *gac_review_summary_line(const json_t *payload) {
    if (!is_obj(payload)) return NULL;
    const json_t *text = json_obj_get(payload, "text");
    const char *t = jtruth(text) ? jstr(text) : "";
    char *stripped = strip_ws(t ? t : "");
    if (!stripped[0]) { free(stripped); return NULL; }
    char *out = xsf("  %s", stripped);
    free(stripped);
    return out;
}

/* ── _summarize_api_error ── */
/* PoP: gac_summarize_api_error @ hermes_cli/gateway_agent.py:_summarize_api_error */
static char *gac_summarize_api_error(const json_t *exc) {
    const char *msg = json_get_str(exc, "msg", "");
    size_t n = strlen(msg);
    if (n > 300) n = 300;
    char *out = malloc(n + 1);
    memcpy(out, msg, n);
    out[n] = '\0';
    return out;
}

/* ── _rate_limit_state (module flow; ctor seams) ── */
/* PoP: gac_rate_limit_state @ hermes_cli/gateway_agent.py:_rate_limit_state */
static json_t *gac_rate_limit_state(const json_t *data, const gac_seams_t *s) {
    if (!is_obj(data)) return NULL;
    static const char *BUCKETS[] = {"requests_min", "requests_hour", "tokens_min", "tokens_hour"};
    static const char *ALLOWED[] = {"limit", "remaining", "reset_seconds", "captured_at"};
    json_t *buckets = json_object();
    for (size_t b = 0; b < 4; b++) {
        const json_t *bd = json_obj_get(data, BUCKETS[b]);
        json_t *filtered = json_object();
        if (is_obj(bd)) {
            for (size_t i = 0; i < bd->c.count; i++) {
                const char *key = bd->c.keys[i];
                bool ok = false;
                for (size_t a = 0; a < 4; a++) if (!strcmp(key, ALLOWED[a])) { ok = true; break; }
                if (ok) json_set(filtered, key, json_copy(bd->c.items[i]));
            }
        }
        json_t *a = args0();
        json_set(a, "kwargs", filtered);  /* owns filtered */
        json_t *r = gac_call(s, "RateLimitBucket", a);
        json_free(a);
        if (gac_raise_class(r)) { json_free(buckets); gac_park(r); return NULL; }
        json_set(buckets, BUCKETS[b], r ? r : json_null());
    }
    const json_t *ca = json_obj_get(data, "captured_at");
    double cav = jtruth(ca) ? gac_num(ca) : 0.0;
    const json_t *pv = json_obj_get(data, "provider");
    const char *ps = jtruth(pv) ? jstr(pv) : "";
    json_t *a = args0();
    json_set(a, "buckets", buckets);  /* owns buckets */
    aset_n(a, "captured_at", cav);
    aset_s(a, "provider", ps ? ps : "");
    json_t *r = gac_call(s, "RateLimitState", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return NULL; }
    return r;
}

/* ── _moa_handler (the closure body; kind is the factory arg) ── */
/* PoP: gac_moa_handler @ hermes_cli/gateway_agent.py:_moa_handler */
static void gac_moa_handler(const char *kind, const json_t *payload, bool cb_present,
                            const gac_seams_t *s) {
    if (!cb_present) return;  /* agent.tool_progress_callback is None */
    static const char *EXCLUDED[] = {"label", "text", "name", "preview", "index", "count"};
    json_t *extra = json_object();
    if (is_obj(payload)) {
        for (size_t i = 0; i < payload->c.count; i++) {
            const char *key = payload->c.keys[i];
            bool ex = false;
            for (size_t e = 0; e < 6; e++) if (!strcmp(key, EXCLUDED[e])) { ex = true; break; }
            if (!ex) json_set(extra, key, json_copy(payload->c.items[i]));
        }
    }
    const json_t *idx = is_obj(payload) ? json_obj_get(payload, "index") : NULL;
    if (jpresent(idx)) json_set(extra, "moa_index", json_copy(idx));
    const json_t *cnt = is_obj(payload) ? json_obj_get(payload, "count") : NULL;
    if (jpresent(cnt)) json_set(extra, "moa_count", json_copy(cnt));
    const json_t *label = is_obj(payload) ? json_obj_get(payload, "label") : NULL;
    if (!jtruth(label)) label = is_obj(payload) ? json_obj_get(payload, "name") : NULL;
    const json_t *text = is_obj(payload) ? json_obj_get(payload, "text") : NULL;
    if (!jtruth(text)) text = is_obj(payload) ? json_obj_get(payload, "preview") : NULL;
    json_t *a = args0();
    aset_s(a, "kind", kind);
    aset_j(a, "label", label);
    aset_j(a, "text", text);
    json_set(a, "extra", extra);  /* owns extra */
    json_t *r = gac_call(s, "tool_progress_callback", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return; }
    json_free(r);
}

/* ── _apply_info ── */
/* PoP: gac_apply_info @ hermes_cli/gateway_agent.py:_apply_info */
static void gac_apply_info(json_t *st, const json_t *info) {
    if (!jtruth(info)) return;  /* info or {} */
    const json_t *m = json_obj_get(info, "model");
    if (jtruth(m)) json_set(st, "model", json_copy(m));
    const json_t *p = json_obj_get(info, "provider");
    if (jtruth(p)) json_set(st, "provider", json_copy(p));
    const json_t *cl = json_obj_get(info, "context_length");
    if (jtruth(cl)) {
        json_t *comp = json_obj_get(st, "context_compressor");
        if (is_obj(comp) && gac_is_num(cl))
            json_set(comp, "context_length", json_number(trunc(gac_num(cl))));
    }
}

/* ── tools (property) ── */
/* PoP: gac_tools @ hermes_cli/gateway_agent.py:tools */
static json_t *gac_tools(json_t *st) {
    const json_t *info = state_info(st);
    const json_t *groups = info ? json_obj_get(info, "tools") : NULL;
    json_t *out = json_array();
    if (is_obj(groups)) {
        for (size_t g = 0; g < groups->c.count; g++) {
            const json_t *members = groups->c.items[g];
            if (!is_arr(members)) continue;  /* members or [] */
            for (size_t i = 0; i < members->c.count; i++) {
                json_t *fn = json_object();
                json_set(fn, "name", json_copy(members->c.items[i]));
                json_t *tool = json_object();
                json_set(tool, "type", json_string("function"));
                json_set(tool, "function", fn);
                json_append(out, tool);
            }
        }
    }
    return out;
}

/* ── valid_tool_names (property; composes tools REAL) ── */
/* PoP: gac_valid_tool_names @ hermes_cli/gateway_agent.py:valid_tool_names */
static json_t *gac_valid_tool_names(json_t *st) {
    json_t *tools = gac_tools(st);
    /* set of names -> sorted unique array (normalized on both sides) */
    json_t *names = json_array();
    for (size_t i = 0; i < tools->c.count; i++) {
        const json_t *fn = json_obj_get(tools->c.items[i], "function");
        const char *nm = jstr(fn ? json_obj_get(fn, "name") : NULL);
        if (!nm) continue;
        bool dup = false;
        for (size_t j = 0; j < names->c.count; j++) {
            const char *e = jstr(names->c.items[j]);
            if (e && !strcmp(e, nm)) { dup = true; break; }
        }
        if (!dup) json_append(names, json_string(nm));
    }
    json_free(tools);
    /* insertion-sort the unique names (ASCII strcmp order) */
    for (size_t i = 1; i < names->c.count; i++) {
        json_t *key = names->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(jstr(names->c.items[j - 1]), jstr(key)) > 0) {
            names->c.items[j] = names->c.items[j - 1];
            j--;
        }
        names->c.items[j] = key;
    }
    return names;
}

/* ── apply_session_usage ── */
/* PoP: gac_apply_session_usage @ hermes_cli/gateway_agent.py:apply_session_usage */
static void gac_apply_session_usage(json_t *st, const json_t *usage, const gac_seams_t *s) {
    if (!is_obj(usage)) return;
    for (size_t i = 0; i < COUNTER_N; i++) {
        const json_t *v = json_obj_get(usage, COUNTER_PAIRS[i][1]);
        if (gac_is_int(v) && !is_bool(v)) json_set(st, COUNTER_PAIRS[i][0], json_copy(v));
    }
    json_t *comp = json_obj_get(st, "context_compressor");
    const json_t *lpt = json_obj_get(usage, "last_prompt_tokens");
    if (gac_is_int(lpt) && is_obj(comp)) json_set(comp, "last_prompt_tokens", json_copy(lpt));
    const json_t *cl = json_obj_get(usage, "context_length");
    if (jtruth(cl) && is_obj(comp) && gac_is_num(cl))
        json_set(comp, "context_length", json_number(trunc(gac_num(cl))));
    if (json_has(usage, "config_context_length")) {
        const json_t *pin = json_obj_get(usage, "config_context_length");
        json_t *pv = gac_is_int(pin) ? json_copy(pin) : json_null();
        if (is_obj(comp)) json_set(comp, "_config_context_length", json_copy(pv));
        json_set(st, "_config_context_length", pv);
    }
    const json_t *cc = json_obj_get(usage, "compression_count");
    if (gac_is_int(cc) && is_obj(comp)) json_set(comp, "compression_count", json_copy(cc));
    const json_t *source = json_obj_get(usage, "context_source");
    if (jpresent(source) && is_obj(comp)) {
        const char *ss = jstr(source);
        if (ss && !strcmp(ss, "provider_usage")) {
            json_set(comp, "last_real_prompt_tokens", json_null());
        } else {
            double base = gac_num(json_obj_get(comp, "last_prompt_tokens"));
            json_set(comp, "last_real_prompt_tokens", json_number(base + 1));
        }
    }
    const json_t *ct = json_obj_get(usage, "context_tokens");
    if (gac_is_int(ct)) {
        json_set(st, "_context_tokens", json_copy(ct));
        json_t *a = args0();
        json_t *r = gac_call(s, "_anchor_display", a);
        json_free(a);
        if (gac_raise_class(r)) { gac_park(r); return; }
        json_free(r);
    }
    const json_t *latency = json_obj_get(usage, "avg_latency_s");
    const json_t *tps = json_obj_get(usage, "tokens_per_s");
    if (gac_is_num(latency) && gac_num(latency) > 0 && gac_is_num(tps)) {
        json_t *h1 = json_array();
        json_append(h1, json_number(gac_num(latency)));
        json_set(st, "_api_latency_history", h1);
        json_t *h2 = json_array();
        json_append(h2, json_number(gac_num(tps) * gac_num(latency)));
        json_set(st, "_api_output_history", h2);
    }
    if (json_has(usage, "rate_limit")) {
        const json_t *data = json_obj_get(usage, "rate_limit");
        json_t *a = args0();
        aset_j(a, "data", data);
        json_t *r = gac_call(s, "_rate_limit_state", a);
        json_free(a);
        if (gac_raise_class(r)) { gac_park(r); return; }
        json_set(st, "_rate_limit_state", r ? r : json_null());
    }
    const json_t *m = json_obj_get(usage, "model");
    if (jtruth(m)) json_set(st, "model", json_copy(m));
    const json_t *p = json_obj_get(usage, "provider");
    if (jtruth(p)) json_set(st, "provider", json_copy(p));
}

/* ── _reset_session_figures ── */
/* PoP: gac_reset_session_figures @ hermes_cli/gateway_agent.py:_reset_session_figures */
static void gac_reset_session_figures(json_t *st) {
    for (size_t i = 0; i < COUNTER_N; i++) json_set(st, COUNTER_PAIRS[i][0], json_number(0));
    json_t *comp = json_obj_get(st, "context_compressor");
    if (is_obj(comp)) {
        json_set(comp, "last_prompt_tokens", json_number(0));
        json_set(comp, "last_real_prompt_tokens", json_null());
        json_set(comp, "compression_count", json_number(0));
    }
    json_set(st, "_context_tokens", json_number(0));
    json_set(st, "_turn_calls_base", json_number(0));
    json_set(st, "_turn_base_usage_anchor", json_null());
    json_set(st, "_turn_api_calls", json_null());
    json_set(st, "_api_latency_history", json_array());
    json_set(st, "_api_output_history", json_array());
}

/* ── reset_session_state ── */
/* PoP: gac_reset_session_state @ hermes_cli/gateway_agent.py:reset_session_state */
static void gac_reset_session_state(json_t *st, const gac_seams_t *s) {
    json_t *a = args0();
    json_t *r = gac_call(s, "_reset_session_figures", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return; }
    json_free(r);
    json_t *state = self_state(st);
    const json_t *messages = state ? json_obj_get(state, "messages") : NULL;
    const json_t *usage = state ? json_obj_get(state, "usage") : NULL;
    if (!jtruth(messages) && !jtruth(usage)) return;
    a = args0();
    r = gac_call(s, "_refresh_stats", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return; }
    bool refreshed = jtruth(r);
    json_free(r);
    if (!refreshed && jtruth(usage)) {
        a = args0();
        aset_j(a, "usage", usage);
        r = gac_call(s, "apply_session_usage", a);
        json_free(a);
        if (gac_raise_class(r)) { gac_park(r); return; }
        json_free(r);
    }
}

/* ── interrupt ── */
/* PoP: gac_interrupt @ hermes_cli/gateway_agent.py:interrupt */
static void gac_interrupt(json_t *st, const char *message, const gac_seams_t *s) {
    (void)message;  /* message stays CLI-side */
    json_set(st, "_interrupt_requested", json_bool(true));
    const json_t *sent = json_obj_get(st, "_interrupt_sent_at");
    if (!jpresent(sent)) {
        json_t *a = args0();
        json_t *r = gac_call(s, "monotonic", a);
        json_free(a);
        if (gac_raise_class(r)) { gac_park(r); return; }  /* outside the try */
        json_set(st, "_interrupt_sent_at", r ? r : json_null());
    }
    const json_t *gen = json_obj_get(st, "_generation");
    if (!jpresent(gen)) {
        json_t *state = self_state(st);
        gen = state ? json_obj_get(state, "execution_generation") : NULL;
    }
    const char *sid = json_get_str(st, "session_id", "");
    json_t *a = args0();
    aset_s(a, "method", "session.interrupt");
    aset_s(a, "session_id", sid);
    aset_j(a, "execution_generation", gen);
    aset_n(a, "timeout", 5);
    json_t *r = gac_call(s, "call", a);
    json_free(a);
    const char *kc = gac_raise_class(r);
    if (kc) {
        if (gac_is_base(kc)) { gac_park(r); return; }  /* BaseException passes */
        json_free(r);  /* except Exception: logger.debug, swallow */
        return;
    }
    json_free(r);
}

/* ── hard_interrupt ── */
/* PoP: gac_hard_interrupt @ hermes_cli/gateway_agent.py:hard_interrupt */
static void gac_hard_interrupt(json_t *st, const char *message, const gac_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "message", message);
    json_t *r = gac_call(s, "interrupt", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return; }
    json_free(r);
}

/* ── steer ── */
/* PoP: gac_steer @ hermes_cli/gateway_agent.py:steer */
static json_t *gac_steer(json_t *st, const char *text, const gac_seams_t *s) {
    (void)st;
    json_t *a = args0();
    aset_s(a, "verb", "steer");
    aset_s(a, "text", text);
    json_t *r = gac_call(s, "_correct", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return NULL; }
    return r;
}

/* ── redirect ── */
/* PoP: gac_redirect @ hermes_cli/gateway_agent.py:redirect */
static json_t *gac_redirect(json_t *st, const char *text, const gac_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "verb", "redirect");
    aset_s(a, "text", text);
    json_t *r = gac_call(s, "_correct", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return NULL; }
    if (is_bool(r) && r->bool_val) json_set(st, "_turn_ask_index", json_null());
    return r;
}

/* ── _expired ── */
/* PoP: gac_expired @ hermes_cli/gateway_agent.py:_expired */
static bool gac_expired(json_t *st, const gac_seams_t *s) {
    const json_t *sent = json_obj_get(st, "_interrupt_sent_at");
    if (!jpresent(sent)) return false;
    json_t *a = args0();
    json_t *r = gac_call(s, "monotonic", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return false; }
    double now = gac_num(r);
    json_free(r);
    return (now - gac_num(sent)) > 10.0;  /* INTERRUPT_SETTLE_SECONDS */
}

/* ── _stage_content_parts ── */
static bool gac_is_linebreak(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f'
           || c == '\x1c' || c == '\x1d' || c == '\x1e';
}
/* Python str.splitlines() over the ASCII line-boundary set. */
static json_t *gac_splitlines(const char *s) {
    json_t *lines = json_array();
    const char *p = s ? s : "";
    while (*p) {
        const char *start = p;
        while (*p && !gac_is_linebreak(*p)) p++;
        size_t n = (size_t)(p - start);
        char *ln = malloc(n + 1);
        memcpy(ln, start, n);
        ln[n] = '\0';
        json_append(lines, json_string(ln));
        free(ln);
        if (*p) {
            if (*p == '\r') { p++; if (*p == '\n') p++; }
            else p++;
        }
    }
    return lines;
}
/* one text part: splitlines, drop "[Image attached" lines, join "\n", strip */
static char *gac_clean_text_part(const char *text) {
    json_t *lines = gac_splitlines(text ? text : "");
    size_t cap = 1;
    for (size_t i = 0; i < lines->c.count; i++) {
        const char *ln = jstr(lines->c.items[i]);
        if (ln && strncmp(ln, "[Image attached", 15)) cap += strlen(ln) + 1;
    }
    char *buf = malloc(cap);
    buf[0] = '\0';
    bool first = true;
    for (size_t i = 0; i < lines->c.count; i++) {
        const char *ln = jstr(lines->c.items[i]);
        if (ln && strncmp(ln, "[Image attached", 15)) {
            if (!first) strcat(buf, "\n");
            strcat(buf, ln);
            first = false;
        }
    }
    json_free(lines);
    char *stripped = strip_ws(buf);
    free(buf);
    return stripped;
}
/* PoP: gac_stage_content_parts @ hermes_cli/gateway_agent.py:_stage_content_parts */
static json_t *gac_stage_content_parts(json_t *st, const json_t *parts, const gac_seams_t *s) {
    json_t *texts = json_array();
    json_t *attachments = json_array();
    const char *sid = json_get_str(st, "session_id", "");
    if (is_arr(parts)) {
        for (size_t i = 0; i < parts->c.count; i++) {
            const json_t *part = parts->c.items[i];
            if (!is_obj(part)) continue;
            const char *ty = jstr(json_obj_get(part, "type"));
            if (ty && !strcmp(ty, "text")) {
                const json_t *tv = json_obj_get(part, "text");
                const char *t = jstr(tv);
                char *cleaned = gac_clean_text_part(t ? t : "");
                json_append(texts, json_string(cleaned));
                free(cleaned);
            } else if (ty && !strcmp(ty, "image_url")) {
                const json_t *iu = json_obj_get(part, "image_url");
                const json_t *url = is_obj(iu) ? json_obj_get(iu, "url") : NULL;
                const char *u = jstr(url);
                if (u && !strncmp(u, "data:image/", 11)) {
                    json_t *a = args0();
                    aset_s(a, "method", "image.attach_bytes");
                    aset_s(a, "session_id", sid);
                    aset_s(a, "content_base64", u);
                    json_t *staged = gac_call(s, "call", a);
                    json_free(a);
                    if (gac_raise_class(staged)) {
                        json_free(texts); json_free(attachments); gac_park(staged); return NULL;
                    }
                    const json_t *path = is_obj(staged) ? json_obj_get(staged, "path") : NULL;
                    const json_t *mime = is_obj(staged) ? json_obj_get(staged, "mime") : NULL;
                    json_t *att = json_object();
                    json_set(att, "path", path ? json_copy(path) : json_null());
                    json_set(att, "mime", mime ? json_copy(mime) : json_null());
                    json_append(attachments, att);
                    json_free(staged);
                }
            }
        }
    }
    /* "\n\n".join(t for t in texts if t) */
    size_t cap = 1;
    for (size_t i = 0; i < texts->c.count; i++) {
        const char *t = jstr(texts->c.items[i]);
        if (t && t[0]) cap += strlen(t) + 2;
    }
    char *joined = malloc(cap);
    joined[0] = '\0';
    bool first = true;
    for (size_t i = 0; i < texts->c.count; i++) {
        const char *t = jstr(texts->c.items[i]);
        if (t && t[0]) {
            if (!first) strcat(joined, "\n\n");
            strcat(joined, t);
            first = false;
        }
    }
    json_free(texts);
    json_t *out = json_array();
    json_append(out, json_string(joined));
    json_append(out, attachments);
    free(joined);
    return out;
}

/* ── _lost_turn_notice ── */
/* PoP: gac_lost_turn_notice @ hermes_cli/gateway_agent.py:_lost_turn_notice */
static char *gac_lost_turn_notice(json_t *st, const gac_seams_t *s) {
    json_t *state = self_state(st);
    const json_t *pending = state ? json_obj_get(state, "pending") : NULL;
    bool any_unknown = false;
    if (is_arr(pending)) {
        for (size_t i = 0; i < pending->c.count; i++) {
            const json_t *row = pending->c.items[i];
            const char *status = jstr(is_obj(row) ? json_obj_get(row, "status") : NULL);
            if (status && !strcmp(status, "unknown")) { any_unknown = true; break; }
        }
    }
    if (!any_unknown) return strdup("");
    json_t *a = args0();
    aset_j(a, "pending", pending);
    json_t *r = gac_call(s, "pending_lines", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return NULL; }
    size_t cap = 1;
    if (is_arr(r)) for (size_t i = 0; i < r->c.count; i++) {
        const char *t = jstr(json_get(r, i));
        cap += (t ? strlen(t) : 0) + 1;
    }
    char *joined = malloc(cap);
    joined[0] = '\0';
    if (is_arr(r)) for (size_t i = 0; i < r->c.count; i++) {
        if (i) strcat(joined, "\n");
        const char *t = jstr(json_get(r, i));
        strcat(joined, t ? t : "");
    }
    json_free(r);
    return joined;
}

/* ── _user_row ── */
/* PoP: gac_user_row @ hermes_cli/gateway_agent.py:_user_row */
static json_t *gac_user_row(json_t *st, const json_t *user_message) {
    const json_t *staged = json_obj_get(st, "_pending_cli_user_message");
    if (is_obj(staged)) return json_copy(staged);
    json_t *out = json_object();
    json_set(out, "role", json_string("user"));
    json_set(out, "content", user_message ? json_copy(user_message) : json_null());
    return out;
}

/* ── _failed (composes _user_row REAL) ── */
/* PoP: gac_failed @ hermes_cli/gateway_agent.py:_failed */
static json_t *gac_failed(json_t *st, const json_t *history, const json_t *user_message,
                          const char *error, const char *failure_reason, bool retryable) {
    json_t *out = result_defaults();
    json_set(out, "final_response", json_string(""));
    json_t *messages = is_arr(history) ? json_copy(history) : json_array();
    json_append(messages, gac_user_row(st, user_message));
    json_set(out, "messages", messages);
    json_set(out, "failed", json_bool(true));
    json_set(out, "completed", json_bool(false));
    json_set(out, "error", error ? json_string(error) : json_null());
    json_set(out, "session_id", json_string(json_get_str(st, "session_id", "")));
    if (failure_reason && failure_reason[0]) {
        json_set(out, "failure_reason", json_string(failure_reason));
        json_set(out, "failure_retryable", json_bool(retryable));
    }
    return out;
}

/* ── _unadmitted (composes _failed REAL; seams _give_back_turn_route) ── */
/* PoP: gac_unadmitted @ hermes_cli/gateway_agent.py:_unadmitted */
static json_t *gac_unadmitted(json_t *st, const json_t *history, const json_t *user_message,
                              const char *error, const gac_seams_t *s) {
    json_t *a = args0();
    json_t *r = gac_call(s, "_give_back_turn_route", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return NULL; }
    json_free(r);
    json_t *out = gac_failed(st, history, user_message, error, "gateway_connection", false);
    json_set(out, "messages", is_arr(history) ? json_copy(history) : json_array());
    json_set(out, "admitted", json_bool(false));
    return out;
}

/* ── _emit_print ── */
/* PoP: gac_emit_print @ hermes_cli/gateway_agent.py:_emit_print */
static void gac_emit_print(json_t *st, const char *text, const gac_seams_t *s) {
    if (json_get_bool(st, "suppress_status_output", false)) return;
    const json_t *pf = json_obj_get(st, "_print_fn");
    json_t *a = args0();
    aset_s(a, "text", text);
    json_t *r = gac_call(s, jpresent(pf) ? "print_fn" : "print", a);
    json_free(a);
    if (gac_raise_class(r)) { gac_park(r); return; }
    json_free(r);
}

/* ── _on_print (composes _emit_print REAL) ── */
/* PoP: gac_on_print @ hermes_cli/gateway_agent.py:_on_print */
static void gac_on_print(json_t *st, const json_t *payload, const gac_seams_t *s) {
    const json_t *text = is_obj(payload) ? json_obj_get(payload, "text") : NULL;
    const char *t = jtruth(text) ? jstr(text) : "";
    gac_emit_print(st, t ? t : "", s);
}

/* ── _on_review_summary (composes review_summary_line + _emit_print REAL) ── */
/* PoP: gac_on_review_summary @ hermes_cli/gateway_agent.py:_on_review_summary */
static void gac_on_review_summary(json_t *st, const json_t *payload, const gac_seams_t *s) {
    char *line = gac_review_summary_line(payload);
    if (line && line[0]) gac_emit_print(st, line, s);
    free(line);
}

/* ── _owns (turn identity modeled by id) ── */
/* PoP: gac_owns @ hermes_cli/gateway_agent.py:_owns */
static bool gac_owns(json_t *st, const json_t *turn) {
    const char *turn_id = json_get_str(turn, "id", "");
    const char *cur = jstr(json_obj_get(st, "_turn"));
    bool same_turn = cur && !strcmp(cur, turn_id);
    const char *sid = json_get_str(st, "session_id", "");
    const char *tsid = json_get_str(turn, "session_id", "");
    return same_turn && !strcmp(sid, tsid);
}