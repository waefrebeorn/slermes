/*
 * t_port_run_busy_flow.c — oracle harness for the gateway/run_busy.py
 * busy/queue-admission FLOW cores ported in src/gateway/run_busy_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order (state seams return
 * BORROWED mirror nodes; MessageEvent registers its scripted event), and the
 * harness emits {"func", "in", "out": {"ret", "trace", "state"}} per case.
 * tests/oracle_run_busy_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/run_busy_flow.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *state;    /* owned mirror */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} rbf_script_ctx;

static void rbf_rec(rbf_script_ctx *c, json_t *ev) {
    json_append(c->trace, ev);
}

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object return; anything else = constant. Absent/exhausted ->
 * NULL (the oracle raises on absent scripts, surfacing fixture gaps). */
static const json_t *rbf_next_script(rbf_script_ctx *c, const char *name) {
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) {
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= json_len(v)) return NULL;
        json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *rbf_state_node(rbf_script_ctx *c, const char *sect, const char *key) {
    json_t *s = json_obj_get(c->state, sect);
    return s ? json_obj_get(s, key) : NULL;
}

static json_t *rbf_stub_call(void *ud, const char *name, const json_t *args) {
    rbf_script_ctx *c = ud;
    /* State seams: record a call-time snapshot, return the BORROWED node. */
    if (strcmp(name, "_peek_session_state") == 0 || strcmp(name, "_session_state") == 0) {
        const char *key = json_get_str(args, "session_key", "");
        json_t *node = rbf_state_node(c, "sessions", key);
        if (!node && strcmp(name, "_session_state") == 0) {
            node = json_object();
            json_t *conv = json_object();
            json_set(conv, "queued_events", json_array());
            json_set(node, "conversation", conv);
            json_set(node, "turn", json_object());
            json_set(json_obj_get(c->state, "sessions"), key, node);
        }
        json_t *ev = gflow_ev(name);
        gflow_evj(ev, "args", args);
        gflow_evj(ev, "ret", node);
        rbf_rec(c, ev);
        return node;   /* borrowed — the port never frees state-seam results */
    }
    if (strcmp(name, "log") == 0) {
        json_t *ev = gflow_ev("log");
        gflow_evj(ev, "level", json_obj_get(args, "level"));
        gflow_evj(ev, "fmt", json_obj_get(args, "fmt"));
        gflow_evj(ev, "args", json_obj_get(args, "args"));
        rbf_rec(c, ev);
        return NULL;
    }
    const json_t *script = rbf_next_script(c, name);
    if (strcmp(name, "MessageEvent") == 0) {
        json_t *ev = gflow_ev(name);
        gflow_evj(ev, "args", args);
        if (script && script->type == JSON_OBJECT && !json_obj_get(script, "__raise__")) {
            const char *eid = json_get_str(script, "id", "");
            json_set(json_obj_get(c->state, "events"), eid, json_copy(script));
            gflow_evs(ev, "ret", eid);
            rbf_rec(c, ev);
            return json_string(eid);
        }
        gflow_evj(ev, "ret", script);
        rbf_rec(c, ev);
        return script ? json_copy(script) : NULL;
    }
    json_t *ev = gflow_ev(name);
    gflow_evj(ev, "args", args);
    gflow_evj(ev, "ret", script);
    rbf_rec(c, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

typedef json_t *(*rbf_entry_fn)(rbf_env *, const json_t *);
typedef struct { const char *name; rbf_entry_fn fn; } rbf_dispatch;

static const rbf_dispatch rbf_table[] = {
    {"_strip_slot", rbf_strip_slot},
    {"_same_chat_key_slots", rbf_same_chat_key_slots},
    {"_queue_during_drain_enabled", rbf_queue_during_drain_enabled},
    {"_overflow_queue", rbf_overflow_queue},
    {"_enqueue_fifo", rbf_enqueue_fifo},
    {"_requeue_event_at_front", rbf_requeue_event_at_front},
    {"_take_wake_admitted_before_drain", rbf_take_wake_admitted_before_drain},
    {"_queue_depth", rbf_queue_depth},
    {"_merge_leftover_steer", rbf_merge_leftover_steer},
    {"_defer_past_exhaustion_reset", rbf_defer_past_exhaustion_reset},
    {"_requeue_at_depth_cap", rbf_requeue_at_depth_cap},
    {"_carry_over_event", rbf_carry_over_event},
    {"_is_goal_continuation_event", rbf_is_goal_continuation_event},
    {"_steer_active_subagents", rbf_steer_active_subagents},
    {"_try_agent_verb", rbf_try_agent_verb},
    {"_busy_slash_route", rbf_busy_slash_route},
    {"_busy_interrupt_and_ack", rbf_busy_interrupt_and_ack},
    {"_command_turn_admitted", rbf_command_turn_admitted},
};

static void rbf_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!state || state->type != JSON_OBJECT) {
        json_free(state);
        state = json_object();
    }
    static const char *sects[] = {"events", "agents", "sessions", "adapters"};
    for (int i = 0; i < 4; i++) {
        if (!json_obj_get(state, sects[i]))
            json_set(state, sects[i], json_object());
    }
    rbf_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    rbf_env env = { .state = state, .call = rbf_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(rbf_table) / sizeof(rbf_table[0]); i++) {
        if (strcmp(func, rbf_table[i].name) == 0) {
            ret = rbf_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) {
        json_set(out, "error", json_string("unknown func"));
    }
    if (env.exc) {   /* propagated BaseException / unhandled raise */
        json_free(ret);
        ret = env.exc;
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", json_copy(state));
    json_free(state);
    json_free(ctx.pos);
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
        rbf_run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
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