/*
 * t_port_run_busy_flow2.c — oracle harness for the 12 remainder FLOW cores
 * ported in src/gateway/run_busy_flow2.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the scripted seam results ("io") and the call arguments. The
 * universal seam stub records one {ev,args,ret} trace event per call, in call
 * order. Harness emits {"func","in","out":{"ret","trace"}} per case; the
 * oracle (tests/oracle_run_busy_flow2.py) drives the LIVE Python with the same
 * scripts and diffs the two.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/run_busy_flow2.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} rb2_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object return; anything else = constant. A raise marker is a
 * constant object. Absent/exhausted -> NULL (None); the oracle raises on an
 * absent script, surfacing fixture gaps. */
static const json_t *rb2_next_script(rb2_script_ctx *c, const char *name) {
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

static void rb2_rec(rb2_script_ctx *c, json_t *ev) {
    json_append(c->trace, ev);
}

static json_t *rb2_stub_call(void *ud, const char *name, const json_t *args) {
    rb2_script_ctx *c = ud;
    if (strcmp(name, "log") == 0) {
        json_t *ev = gflow_ev("log");
        gflow_evj(ev, "level", json_obj_get(args, "level"));
        gflow_evj(ev, "fmt", json_obj_get(args, "fmt"));
        gflow_evj(ev, "args", json_obj_get(args, "args"));
        rb2_rec(c, ev);
        return NULL;
    }
    const json_t *script = rb2_next_script(c, name);
    json_t *ev = gflow_ev(name);
    gflow_evj(ev, "args", args);
    gflow_evj(ev, "ret", script);
    rb2_rec(c, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

typedef json_t *(*rb2_entry_fn)(rb2_env *, const json_t *);
typedef struct { const char *name; rb2_entry_fn fn; } rb2_dispatch;

static const rb2_dispatch rb2_table[] = {
    {"approval_input_words", rb2_approval_input_words},
    {"_tail_has_slot", rb2_tail_has_slot},
    {"_destructive_optout_note", rb2_destructive_optout_note},
    {"_busy_reply_to", rb2_busy_reply_to},
    {"_demote_interrupt", rb2_demote_interrupt},
    {"_command_handler_table", rb2_command_handler_table},
    {"_gateway_idle_command_handlers", rb2_gateway_idle_handlers},
    {"_plaintext_approval_words", rb2_plaintext_approval_words},
    {"_get_max_concurrent_sessions", rb2_get_max_concurrent_sessions},
    {"_active_session_limit_message", rb2_active_session_limit_message},
    {"_route_stop_epoch", rb2_route_stop_epoch},
    {"_send_busy_ack_reply", rb2_send_busy_ack_reply},
};

static void rb2_run_case(const char *func, const json_t *in, json_t *out) {
    rb2_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"),
        .trace = json_array(), .pos = json_object(),
    };
    rb2_env env = { .call = rb2_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(rb2_table) / sizeof(rb2_table[0]); i++) {
        if (strcmp(func, rb2_table[i].name) == 0) {
            ret = rb2_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    if (env.exc) { json_free(ret); ret = env.exc; }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
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
        rb2_run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *fn = json_string(func);
        char *fn_s = json_serialize(fn);
        json_free(fn);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", fn_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(fn_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
