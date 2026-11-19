/*
 * t_port_turn_progress_flow.c — oracle harness for the
 * gateway/run_turn_progress.py progress-reporting FLOW cores ported in
 * src/gateway/turn_progress_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state": st/ctx/adapters/consumers),
 * the scripted seam results ("io") and the call arguments. The universal seam
 * stub records one {ev, args, ret} trace event per call in call order and
 * returns the script (NULL == None; {"__raise__":...} == exception). The
 * harness emits {"func", "in", "out": {"ret", "trace", "state"}} per case.
 * tests/oracle_turn_progress_flow.py drives the LIVE Python with the same
 * scripts and compares ordered trace + return + final state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/turn_progress_flow.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *state;    /* owned mirror */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} tpf_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object return; anything else = constant. Absent/exhausted ->
 * NULL (the oracle raises on absent scripts, surfacing fixture gaps). */
static const json_t *tpf_next_script(tpf_script_ctx *c, const char *name) {
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

static json_t *tpf_stub_call(void *ud, const char *name, const json_t *args) {
    tpf_script_ctx *c = ud;
    if (strcmp(name, "log") == 0) {
        json_t *ev = gflow_ev("log");
        gflow_evj(ev, "level", json_obj_get(args, "level"));
        gflow_evj(ev, "fmt", json_obj_get(args, "fmt"));
        gflow_evj(ev, "args", json_obj_get(args, "args"));
        json_append(c->trace, ev);
        return NULL;
    }
    const json_t *script = tpf_next_script(c, name);
    json_t *ev = gflow_ev(name);
    gflow_evj(ev, "args", args);
    gflow_evj(ev, "ret", script);
    json_append(c->trace, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

typedef json_t *(*tpf_entry_fn)(tpf_env *, const json_t *);
typedef struct { const char *name; tpf_entry_fn fn; } tpf_dispatch;

static const tpf_dispatch tpf_table[] = {
    {"_append_progress_repeat_count", tpf_append_progress_repeat_count},
    {"_note_flood", tpf_note_flood},
    {"_in_flood_window", tpf_in_flood_window},
    {"_owed_send_failed", tpf_owed_send_failed},
    {"_settle_progress_send", tpf_settle_progress_send},
    {"_hold_partial_tail", tpf_hold_partial_tail},
    {"_settle_partial_progress", tpf_settle_partial_progress},
    {"_owed_tail_blocks", tpf_owed_tail_blocks},
    {"_progress_emit", tpf_progress_emit},
    {"_progress_absorb", tpf_progress_absorb},
    {"_is_reset_marker", tpf_is_reset_marker},
    {"_reset_progress_bubble", tpf_reset_progress_bubble},
    {"_progress_send_or_edit", tpf_progress_send_or_edit},
    {"_roll_progress_overflow_if_needed", tpf_roll_progress_overflow_if_needed},
    {"_deliver_flood_pending_progress", tpf_deliver_flood_pending_progress},
    {"_progress_build_message", tpf_progress_build_message},
    {"progress_callback", tpf_progress_callback},
    {"_status_callback_sync", tpf_status_callback_sync},
};

static void tpf_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!state || state->type != JSON_OBJECT) {
        json_free(state);
        state = json_object();
    }
    static const char *sects[] = {"st", "ctx", "adapters", "consumers"};
    for (int i = 0; i < 4; i++) {
        json_t *s = json_obj_get(state, sects[i]);
        if (!s) json_set(state, sects[i], json_object());
    }
    tpf_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    tpf_env env = { .state = state, .call = tpf_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(tpf_table) / sizeof(tpf_table[0]); i++) {
        if (strcmp(func, tpf_table[i].name) == 0) {
            ret = tpf_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
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
        tpf_run_case(func, in ? in : empty, out);
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
