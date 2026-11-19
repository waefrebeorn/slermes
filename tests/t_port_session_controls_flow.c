/*
 * t_port_session_controls_flow.c — oracle harness for the
 * gateway/session_controls.py control/interrupt FLOW cores ported in
 * src/gateway/session_controls_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io"), the connection double ("self"), the SessionRef ("ref") and the call
 * arguments. The universal seam stub records one {ev, args, ret} trace event
 * per call in call order; the harness emits
 * {"func", "in", "out": {"ret", "trace", "state"}} per case.
 * tests/oracle_session_controls_flow.py drives the LIVE Python with the same
 * scripts.
 *
 * Script semantics (shared with the oracle): a bare JSON array is ONE RESULT
 * PER CALL (last value repeats); an empty array is a constant None;
 * {"__script__": true, "ret": v} is a constant; anything else is a constant;
 * absent == None. A list-valued seam result must be scripted one level deeper.
 * {"__raise__": kind, "message": m} scripts a raised exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/session_controls_flow.c"

typedef struct {
    const json_t *in;
    const json_t *io;    /* borrowed in["io"] */
    json_t *state;       /* owned mirror */
    json_t *trace;       /* owned array */
    json_t *pos;         /* owned per-name queue positions */
} scf_script_ctx;

/* Script lookup: bare array = per-call queue (last repeats); empty array =
 * constant None; {"__script__":true,"ret":v} = constant; else constant. */
static const json_t *scf_next_script(scf_script_ctx *c, const char *name) {
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) {
        if (v->c.count == 0) return NULL;
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= v->c.count) i = v->c.count - 1;
        else json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *scf_stub_call(void *ud, const char *name, const json_t *args) {
    scf_script_ctx *c = ud;
    if (strcmp(name, "log") == 0) {
        json_t *ev = gflow_ev("log");
        gflow_evj(ev, "level", json_obj_get(args, "level"));
        gflow_evj(ev, "fmt", json_obj_get(args, "fmt"));
        gflow_evj(ev, "args", json_obj_get(args, "args"));
        if (json_has(args, "exc_info"))
            gflow_evj(ev, "exc_info", json_obj_get(args, "exc_info"));
        json_append(c->trace, ev);
        return NULL;
    }
    const json_t *script = scf_next_script(c, name);
    /* detach side effect: the authority may evict the subscription mid-call
     * (the live re-check after authority.detach reads the evicted map). */
    if (strcmp(name, "detach") == 0 && c->io
            && json_get_bool(c->io, "detach_evicts", false)) {
        const char *sid = json_get_str(args, "session_id", "");
        json_obj_del(json_obj_get(c->state, "subscriptions"), sid);
    }
    json_t *ev = gflow_ev(name);
    gflow_evj(ev, "args", args);
    gflow_evj(ev, "ret", script);
    json_append(c->trace, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

/* ── set-section dump ordering (Python sorted() on str tuples / strs) ────── */
static int scf_cmp_node(const json_t *a, const json_t *b) {
    if (!a || !b) return (a ? 1 : 0) - (b ? 1 : 0);
    if (a->type != b->type) return (int)a->type - (int)b->type;
    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_NUMBER:
            return (a->num_val > b->num_val) - (a->num_val < b->num_val);
        case JSON_BOOL:
            return (int)a->bool_val - (int)b->bool_val;
        case JSON_ARRAY: {
            size_t n = a->c.count < b->c.count ? a->c.count : b->c.count;
            for (size_t i = 0; i < n; i++) {
                int r = scf_cmp_node(a->c.items[i], b->c.items[i]);
                if (r) return r;
            }
            return (a->c.count > b->c.count) - (a->c.count < b->c.count);
        }
        default:
            return 0;
    }
}
static int scf_cmp_item(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa;
    const json_t *b = *(const json_t *const *)pb;
    return scf_cmp_node(a, b);
}
static json_t *scf_sorted_copy(const json_t *arr) {
    json_t *copy = arr ? json_copy(arr) : json_array();
    if (copy && copy->type == JSON_ARRAY && copy->c.count > 1)
        qsort(copy->c.items, copy->c.count, sizeof(json_t *), scf_cmp_item);
    return copy;
}

typedef json_t *(*scf_entry_fn)(gscf_env *, const json_t *);
typedef struct { const char *name; scf_entry_fn fn; } scf_dispatch;

static const scf_dispatch scf_table[] = {
    { "_refusal_data", gscf_refusal_data },
    { "_profile_label", gscf_profile_label },
    { "_seen_storage_refusal", gscf_seen_storage_refusal },
    { "_log_refusal", gscf_log_refusal },
    { "_log_storage_failure", gscf_log_storage_failure },
    { "model_only", gscf_model_only },
    { "display_projection", gscf_display_projection },
    { "_sibling_for", gscf_sibling_for },
    { "awaits_profile_start", gscf_awaits_profile_start },
    { "ping", gscf_ping },
    { "detach", gscf_detach },
    { "events_since", gscf_events_since },
    { "_voice_stop_replay", gscf_voice_stop_replay },
    { "_record_voice_stop", gscf_record_voice_stop },
    { "_typed_voice_stop", gscf_typed_voice_stop },
    { "submission", gscf_submission },
    { "interrupt", gscf_interrupt },
    { "cancel", gscf_cancel },
};

static void scf_ensure_obj(json_t *state, const char *key) {
    const json_t *v = json_obj_get(state, key);
    if (!v || v->type != JSON_OBJECT) {
        json_set(state, key, json_object());
    }
}
static void scf_ensure_arr(json_t *state, const char *key) {
    const json_t *v = json_obj_get(state, key);
    if (!v || v->type != JSON_ARRAY) {
        json_set(state, key, json_array());
    }
}

static void scf_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!state || state->type != JSON_OBJECT) {
        json_free(state);
        state = json_object();
    }
    scf_ensure_arr(state, "storage_refusals");
    scf_ensure_arr(state, "storage_tracebacks");
    scf_ensure_obj(state, "subscriptions");
    scf_ensure_arr(state, "handed_back");
    scf_ensure_arr(state, "reconnects");
    scf_ensure_obj(state, "replay_cursors");
    if (!json_obj_get(state, "consumed_voice_stops"))
        json_set(state, "consumed_voice_stops", json_null());   /* absent attr */
    scf_ensure_obj(state, "siblings");
    scf_ensure_obj(state, "authorities");
    scf_ensure_obj(state, "sessions");
    scf_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    gscf_env env = { .state = state, .call = scf_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(scf_table) / sizeof(scf_table[0]); i++) {
        if (strcmp(func, scf_table[i].name) == 0) {
            ret = scf_table[i].fn(&env, in);
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
    json_set(out, "trace", ctx.trace);   /* adopts */
    /* Out state: the mutable sections only (authorities/sessions are injected
     * inputs, never written by these flows). Sets dump sorted. */
    json_t *st = json_object();
    json_set(st, "storage_refusals", scf_sorted_copy(json_obj_get(state, "storage_refusals")));
    json_set(st, "storage_tracebacks", scf_sorted_copy(json_obj_get(state, "storage_tracebacks")));
    json_set(st, "subscriptions", json_copy(json_obj_get(state, "subscriptions")));
    json_set(st, "handed_back", scf_sorted_copy(json_obj_get(state, "handed_back")));
    json_set(st, "reconnects", scf_sorted_copy(json_obj_get(state, "reconnects")));
    json_set(st, "replay_cursors", json_copy(json_obj_get(state, "replay_cursors")));
    const json_t *cvs = json_obj_get(state, "consumed_voice_stops");
    json_set(st, "consumed_voice_stops", cvs ? json_copy(cvs) : json_null());
    json_set(st, "siblings", json_copy(json_obj_get(state, "siblings")));
    json_set(out, "state", st);   /* adopts */
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
        scf_run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func (bounds-safe); never call
         * json_escape_string (off-by-one heap overflow in libjson). */
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
