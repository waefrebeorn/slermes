/* t_port_conversation_compression_flow2.c — oracle harness for the second-wave
 * FLOW port of agent/conversation_compression.py
 * (src/agent/conversation_compression_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carrier for the in-place
 * functions. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window.
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh conversation_compression_flow2 \
 *           oracle_conversation_compression_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/conversation_compression_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cp2_script_t;

/* drop None-valued object keys recursively (array elements kept); path doubles
 * {"__path__": s} project to the plain string s (PurePosixPath -> str in live). */
static json_t *cp2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        const json_t *p = json_obj_get(v, "__path__");
        if (p && p->type == JSON_STRING) return json_string(p->str_val ? p->str_val : "");
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *val = v->c.items[i];
            if (val && val->type == JSON_NULL) continue;
            json_set(o, v->c.keys[i], cp2_h_strip_nulls(val));
        }
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, cp2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    return json_copy(v);
}

/* normalize path doubles to strings WITHOUT dropping nulls (state / ret echo
 * keeps present-null keys; live norm_state maps PurePosixPath -> str). */
static json_t *cp2_h_norm_paths(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        const json_t *p = json_obj_get(v, "__path__");
        if (p && p->type == JSON_STRING) return json_string(p->str_val ? p->str_val : "");
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) json_set(o, v->c.keys[i], cp2_h_norm_paths(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, cp2_h_norm_paths(v->c.items[i]));
        return a;
    }
    return json_copy(v);
}

static json_t *cp2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cp2_script_t *sc = (cp2_script_t *)ctx;
    const json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    const json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else {
                result = json_get(entry, idx);
                json_set(sc->cursors, name, json_number(cur + 1));
            }
        } else {
            result = entry;
        }
    }
    int failclosed = !present || exhausted;
    const json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        const json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = cp2_xfmt("no script for %s", name);
        retnode = cp2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cp2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cp2_fn_t)(cp2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cp2_fn_t fn; } cp2_dispatch_t;
static const cp2_dispatch_t CP2_DISPATCH[] = {
    { "set_precommit_deadline_monotonic", cp2_set_precommit_deadline_monotonic },
    { "extend_deadline", cp2_extend_deadline },
    { "_should_suppress", cp2_should_suppress },
    { "_touch", cp2_touch },
    { "complete", cp2_complete },
    { "status_emitted", cp2_status_emitted },
    { "start_refresher", cp2_start_refresher },
    { "release_holder_only", cp2_release_holder_only },
    { "release", cp2_release },
    { "restore_compressor", cp2_restore_compressor },
    { "context_engine_session_db", cp2_context_engine_session_db },
    { "close_context_engine_session_db", cp2_close_context_engine_session_db },
    { "_lower_threshold_to_aux_context", cp2_lower_threshold_to_aux_context },
    { "_configured_custom_providers", cp2_configured_custom_providers },
    { "_hold_attempt_telemetry", cp2_hold_attempt_telemetry },
    { "_fence_gated_worker", cp2_fence_gated_worker },
    { "_is_unchanged_snapshot", cp2_is_unchanged_snapshot },
    { "_compression_cancel_requested", cp2_compression_cancel_requested },
    { "_stop_heartbeat", cp2_stop_heartbeat },
    { "_resolve_fallback_prompt", cp2_resolve_fallback_prompt },
};
#define CP2_DISPATCH_N (sizeof(CP2_DISPATCH) / sizeof(CP2_DISPATCH[0]))

/* ---- line reader (\n only; fixtures may carry raw U+2028 inside strings) ---- */
static char *cp2_h_readline(FILE *f)
{
    size_t cap = 4096, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (c == EOF && n == 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

static void cp2_h_run(const char *func, const json_t *in)
{
    cp2_script_t sc;
    sc.io = cp2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cp2_io_t io;
    io.call = cp2_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = cp2_get(in, "state");
    if (cp2_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CP2_DISPATCH_N; i++) {
        if (strcmp(CP2_DISPATCH[i].func, func) == 0) { ret = CP2_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cp2_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    json_t *ret_n = cp2_h_norm_paths(ret);
    json_free(ret);
    json_t *st_n = cp2_h_norm_paths(st);
    json_free(st);
    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret_n);
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st_n);
    char *s = json_serialize(out);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(sc.cursors);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = cp2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cp2_str(cp2_get(rec, "func"));
        const json_t *in = cp2_get(rec, "in");
        if (func && in) cp2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}