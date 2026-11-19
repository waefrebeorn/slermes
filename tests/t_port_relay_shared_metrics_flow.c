/*
 * t_port_relay_shared_metrics_flow.c — oracle harness for the
 * hermes_cli/observability/relay_shared_metrics.py FLOW cores ported in
 * src/hermes_cli/observability/relay_shared_metrics_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_relay_shared_metrics_flow.py drives the LIVE Python with the
 * same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "relay_shared_metrics_flow.h"

typedef struct {
    const json_t *in;
    rsm_flow_trace_t *trace;
    json_t *session;   /* update_model_call: live session state for lookups */
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { rsm_flow_trace_add(c->trace, e); }

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static const json_t *in_arr(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}

/* ── update_model_call seams ── */
static const json_t *um_fields(void *ctx, const json_t *event) {
    script_ctx_t *c = ctx;
    (void)event;
    const json_t *r = in_obj(c, "fields_ret");
    json_t *e = ev("model_call_fields");
    evj(e, "ret", r);
    rec(c, e);
    return r;
}
static const char *um_error_class(void *ctx, const json_t *event) {
    script_ctx_t *c = ctx;
    (void)event;
    const char *r = in_str(c, "error_class_ret", "none");
    json_t *e = ev("model_error_class");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static void um_observe_call(void *ctx, const json_t *fields, const json_t *usage,
                            const json_t *context_length) {
    script_ctx_t *c = ctx;
    json_t *e = ev("observe_call");
    evj(e, "fields", fields);
    evj(e, "usage", usage);
    evj(e, "context_length", context_length);
    rec(c, e);
}
static void um_observe_error(void *ctx, const json_t *fields, const char *error_class) {
    script_ctx_t *c = ctx;
    json_t *e = ev("observe_error");
    evj(e, "fields", fields);
    evs(e, "error_class", error_class);
    rec(c, e);
}
static void um_observe_turn_call(void *ctx, const json_t *model_call, const json_t *event) {
    script_ctx_t *c = ctx;
    (void)event;
    const char *tid = json_get_str(model_call, "task_id", "");
    const json_t *tasks = c->session ? json_obj_get(c->session, "tasks") : NULL;
    const json_t *task = tasks ? json_obj_get(tasks, tid) : NULL;
    json_t *e = ev("observe_turn_call");
    evs(e, "task_id", tid);
    evb(e, "has_task", task != NULL);
    rec(c, e);
}
static const char *um_ttft(void *ctx, const json_t *event) {
    script_ctx_t *c = ctx;
    (void)event;
    const char *r = in_str(c, "ttft_ret", "unknown");
    json_t *e = ev("ttft_bucket");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static void um_finish(void *ctx, json_t *session, const char *key, const char *outcome) {
    script_ctx_t *c = ctx;
    /* trace key as the live (task_id, request_id) tuple */
    const char *tab = strchr(key, '\t');
    json_t *kj = json_array();
    if (tab) {
        size_t n = (size_t)(tab - key);
        char *t = malloc(n + 1);
        if (t) { memcpy(t, key, n); t[n] = '\0'; json_append(kj, json_string(t)); free(t); }
        json_append(kj, json_string(tab + 1));
    } else {
        json_append(kj, json_string(""));
        json_append(kj, json_string(key));
    }
    json_t *e = ev("finish_model_call");
    json_set(e, "key", kj);
    evs(e, "outcome", outcome);
    rec(c, e);
    /* live state effect: pop the located call */
    json_t *calls = session ? json_obj_get(session, "model_calls") : NULL;
    if (calls) json_obj_del(calls, key);
}
static const json_t *um_tokens(void *ctx, const json_t *usage, const json_t *fields) {
    script_ctx_t *c = ctx;
    const json_t *r = json_obj_get(c->in, "tokens_ret");
    json_t *e = ev("model_token_fields");
    evj(e, "usage", usage);
    evs(e, "call_role", "primary");
    evj(e, "fields", fields);
    evj(e, "ret", r);
    rec(c, e);
    return (r && r->type == JSON_OBJECT) ? r : NULL;
}
static void um_mark(void *ctx, const json_t *task, const char *name,
                    const json_t *data, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    json_t *e = ev("mark");
    evs(e, "name", name);
    evj(e, "data", data);
    evs(e, "task_id", task ? json_get_str(task, "task_id", "") : NULL);
    if (in_bool(c, "mark_raises")) {
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return;
    }
    rec(c, e);
}
static void um_log_warning(void *ctx, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", "warning");
    evs(e, "msg", msg);
    rec(c, e);
}

/* ── clock seam ── */
static long long ck_now(void *ctx) {
    script_ctx_t *c = ctx;
    return (long long)json_get_num(c->in, "now_ns", 0);
}

/* Sort a string array in place (set-output normalization: live turn_ids is a
 * set, so its iteration order is not part of the contract). */
static void sort_str_array(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        size_t j = i;
        while (j > 0 && arr->c.items[j - 1]->type == JSON_STRING &&
               v->type == JSON_STRING &&
               strcmp(arr->c.items[j - 1]->str_val, v->str_val) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = v;
    }
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    rsm_flow_trace_t trace = {0};
    script_ctx_t ctx = {in, &trace, NULL};

    if (strcmp(func, "terminal_flags") == 0) {
        const json_t *error = json_obj_get(in, "error");
        if (error && error->type != JSON_OBJECT) error = NULL;
        json_t *r = rsm_flow_terminal_flags(json_obj_get(in, "result"), error);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "session_pair") == 0) {
        json_t *r = rsm_flow_session_pair(in_obj(&ctx, "event"),
                                          in_str(&ctx, "key", "task_id"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "identities_compatible") == 0) {
        bool r = rsm_flow_identities_compatible(in_arr(&ctx, "candidate"),
                                                in_arr(&ctx, "observed"));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "compatible_tool_call_keys") == 0) {
        json_t *r = rsm_flow_compatible_tool_call_keys(
            in_arr(&ctx, "tool_call_keys"), in_str(&ctx, "task_id", ""),
            in_arr(&ctx, "identity"));
        json_set(out, "ret", r ? r : json_array());
    } else if (strcmp(func, "absorb_peak") == 0) {
        json_t *into = json_copy(in_obj(&ctx, "into"));
        if (!into) into = json_object();
        rsm_flow_absorb_peak(into, in_obj(&ctx, "segment"));
        json_set(out, "ret", json_null());
        json_set(out, "state", into);
    } else if (strcmp(func, "admits") == 0) {
        json_t *state = json_copy(in_obj(&ctx, "state"));
        if (!state) state = json_object();
        json_t *session = json_copy(in_obj(&ctx, "session"));
        if (!session) session = json_object();
        json_t *task = json_copy(in_obj(&ctx, "task"));
        if (!task) task = json_object();
        bool r = rsm_flow_admits(state, session, task, in_obj(&ctx, "event"),
                                 in_bool(&ctx, "current"));
        sort_str_array(json_obj_get(task, "turn_ids"));
        json_set(out, "ret", json_bool(r));
        json_set(out, "state", state);
        json_set(out, "task", task);
    } else if (strcmp(func, "announce_cold") == 0) {
        json_t *expected = json_copy(in_arr(&ctx, "expected"));
        if (!expected) expected = json_array();
        bool r = rsm_flow_announce_cold(expected, in_str(&ctx, "key", ""));
        json_set(out, "ret", json_bool(r));
        json_set(out, "state", expected);
    } else if (strcmp(func, "consume_cold") == 0) {
        json_t *expected = json_copy(in_arr(&ctx, "expected"));
        if (!expected) expected = json_array();
        bool r = rsm_flow_consume_cold(expected, in_str(&ctx, "key", ""));
        json_set(out, "ret", json_bool(r));
        json_set(out, "state", expected);
    } else if (strcmp(func, "count_session_turn") == 0) {
        rsm_clock_seams_t s = {.ctx = &ctx, .monotonic_ns = ck_now};
        json_t *session = json_copy(in_obj(&ctx, "session"));
        if (!session) session = json_object();
        rsm_flow_count_session_turn(session, in_obj(&ctx, "task"),
                                    in_str(&ctx, "outcome", "success"), &s);
        json_set(out, "ret", json_null());
        json_set(out, "state", session);
    } else if (strcmp(func, "update_model_call") == 0) {
        rsm_update_seams_t s = {
            .ctx = &ctx,
            .model_call_fields = um_fields,
            .model_error_class = um_error_class,
            .observe_call = um_observe_call,
            .observe_error = um_observe_error,
            .observe_turn_call = um_observe_turn_call,
            .ttft_bucket = um_ttft,
            .finish_model_call = um_finish,
            .model_token_fields = um_tokens,
            .mark = um_mark,
            .log_warning = um_log_warning,
        };
        json_t *session = json_copy(in_obj(&ctx, "session"));
        ctx.session = session;
        rsm_flow_update_model_call(session, in_obj(&ctx, "event"),
                                   in_bool(&ctx, "finish"), &s);
        json_set(out, "ret", json_null());
        json_set(out, "state", session ? session : json_null());
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func: json_escape_string has an
         * off-by-one heap overflow (see t_port_gateway_flow.c). */
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}