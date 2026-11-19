/*
 * t_port_agent_loop_revive.c -- differential harness for the agent_gaps.c /
 * conversation_loop_gaps.c / scheduler_gaps.c revival ports. Reads JSONL cases
 * on stdin, each {"func": <slot>, "in": {...}}, wires a universal recording
 * seam driven by in["io"] scripts, calls the C port, and echoes one line per
 * case:
 *   {"func":..., "in":{...}, "out":{"ret":...,"trace":[...],"state":{...},"raise":...}}
 * tests/oracle_agent_loop_revive.py replays the SAME scripts through the live
 * Python and compares ordered trace + ret + state + raise.
 *
 * Self-contained: #includes the three port .c files (area-qualified so the
 * shared AL_REVIVE_SEAM block is defined once). The fresh included copies win
 * over any stale linked .o via --allow-multiple-definition. Gate (explicit objs):
 *   bash tests/run_one_oracle.sh agent_loop_revive oracle_agent_loop_revive.py "" \
 *        "src/agent/agent_gaps.o src/agent/conversation_loop_gaps.o src/cron/scheduler_gaps.o lib/libjson/json.o"
 */
#include "agent/agent_gaps.c"
#include "agent/conversation_loop_gaps.c"
#include "cron/scheduler_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal recording seam ── */
typedef struct {
    const json_t *io;     /* per-name scripts */
    json_t *trace;        /* ordered events */
    char *names[128];
    size_t idx[128];
    size_t nn;
} al_rec;

static size_t al_rec_slot(al_rec *r, const char *name) {
    for (size_t i = 0; i < r->nn; i++)
        if (strcmp(r->names[i], name) == 0) return i;
    size_t k = r->nn++;
    r->names[k] = strdup(name);
    r->idx[k] = 0;
    return k;
}

static json_t *al_rec_call(void *ctx, const char *name, const json_t *args) {
    al_rec *r = (al_rec *)ctx;
    json_t *script = r->io ? json_obj_get(r->io, name) : NULL;
    json_t *value;
    if (!script) {
        /* fail closed: an unscripted seam is a harness/fixture bug */
        value = json_object();
        json_set(value, "__raise__", json_string("MissingScript"));
        json_set(value, "message", json_string(name));
    } else if (script->type == JSON_OBJECT && json_obj_get(script, "__seq__")) {
        json_t *seq = json_obj_get(script, "__seq__");
        size_t k = al_rec_slot(r, name);
        size_t n = json_len(seq);
        size_t use = r->idx[k] < n ? r->idx[k] : (n ? n - 1 : 0);
        r->idx[k]++;
        value = n ? json_copy(json_get(seq, use)) : json_null();
    } else {
        value = json_copy(script);
    }
    /* record event {ev,args,ret} */
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", json_copy(value));
    json_append(r->trace, ev);
    /* return: NULL pointer for None (NOT a json_null node), copy otherwise */
    if (!value || value->type == JSON_NULL) { json_free(value); return NULL; }
    return value;
}

/* ── dispatch: fixture slot name -> C port call ── */
static json_t *al_dispatch(const char *f, const json_t *in, const al_io *io, json_t *st, al_exc *e) {
#define D(name, ...) if (strcmp(f, name) == 0) return (__VA_ARGS__)
    D("resolve_stream_max_lifetime", stream_max_lifetime_resolve(in, io, st, e));
    D("exceeded", stream_max_lifetime_exceeded(in, io, st, e));
    D("reserved_session_id", runtime_session_child_reserved_session_id(in, io, st, e));
    D("_maybe_grow_local_window", maybe_grow_local_window(in, io, st, e));
    D("_midturn_request_pressure_tokens", midturn_request_pressure_tokens(in, io, st, e));
    D("_pressure_with_real_floor", pressure_with_real_floor(in, io, st, e));
    D("_moa_client_consumes_prepared_request", moa_client_consumes_prepared_request(in, io, st, e));
    D("_is_interpreter_shutdown_error", is_interpreter_shutdown_error(in, io, st, e));
    D("_target_mirror_eligible", target_mirror_eligible(in, io, st, e));
    D("_should_yield_tick_to_fresh_gateway", should_yield_tick_to_fresh_gateway(in, io, st, e));
    D("_inactivity_watchdog_loop", inactivity_watchdog_loop(in, io, st, e));
    D("_detect_gateway_code_skew", detect_gateway_code_skew(in, io, st, e));
#undef D
    return NULL;
}

static void al_run_case(const char *func, json_t *in) {
    al_rec rec;
    memset(&rec, 0, sizeof rec);
    rec.io = al_get(in, "io");
    rec.trace = json_array();
    al_io io = { al_rec_call, &rec };
    json_t *stin = al_get(in, "state");
    json_t *state = stin ? json_copy(stin) : json_object();
    al_exc exc; exc.kind = NULL; exc.msg = NULL;

    json_t *ret = al_dispatch(func, in, &io, state, &exc);

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", rec.trace);
    json_set(out, "state", state);
    if (exc.kind) {
        json_t *rz = json_object();
        json_set(rz, "kind", json_string(exc.kind));
        json_set(rz, "message", json_string(exc.msg ? exc.msg : ""));
        json_set(out, "raise", rz);
    } else {
        json_set(out, "raise", json_null());
    }
    char *ins = json_serialize(in);
    char *outs = json_serialize(out);
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, ins ? ins : "{}", outs ? outs : "{}");
    free(ins); free(outs);
    al_exc_clear(&exc);
    for (size_t i = 0; i < rec.nn; i++) free(rec.names[i]);
    json_free(out); /* frees ret/trace/state/rz children */
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        json_t *rec = json_parse(line, NULL);
        if (!rec) continue;
        const char *func = al_str(rec, "func", "");
        json_t *in = al_get(rec, "in");
        json_t *inc = in ? json_copy(in) : json_object();
        al_run_case(func, inc);
        json_free(inc);
        json_free(rec);
    }
    return 0;
}