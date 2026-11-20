/* t_port_delegate_prefix_gate_flow2.c — oracle harness for the FLOW port of
 * tools/delegate_tool_prefix_gate.py (src/tools/delegate_prefix_gate_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam name -> constant or a
 * per-call array QUEUE) and in.state carries the mutable carriers (child /
 * leader / gate). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":...,"message":...};
 * trace is the ordered seam events {ev,args,ret}; st is the final state window.
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (Python None), never a json_null node.
 *
 * The gate's leader_done is a state flag: is_set/set are silent, event.wait
 * rides a seam; the run() callable's seam name comes from its __callable__
 * marker (default "gate.run_fn").
 *
 * Gate: bash tests/run_one_oracle.sh delegate_prefix_gate_flow2 \
 *           oracle_delegate_prefix_gate_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/delegate_prefix_gate_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name io queue cursors */
    json_t *trace;       /* owned ordered events */
} dpg2_script_t;

/* drop None-valued object keys recursively (matches oracle norm_args). */
static json_t *dpg2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, dpg2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], dpg2_h_strip_nulls(val));
    }
    return o;
}

static json_t *dpg2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    dpg2_script_t *sc = (dpg2_script_t *)ctx;
    const json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    const json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else { result = json_get(entry, idx); json_set(sc->cursors, name, json_number(cur + 1)); }
        } else result = entry;
    }
    int failclosed = !present || exhausted;
    const json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        const json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val) actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = dpg2_xfmt("no script for %s", name);
        retnode = dpg2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else retnode = actual ? json_copy(actual) : json_null();

    /* Exercise the full builtin exception hierarchy on every raise spec: the
     * BaseException trio walks to BaseException (so a future except-Exception
     * site would let it pass), everything else walks under Exception. */
    if (dpg2_is_raise(retnode)) (void)dpg2_exc_is_exception(dpg2_kind(retnode));

    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", dpg2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (dpg2_is_raise(retnode)) return retnode;
    if (!retnode || retnode->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*dpg2_fn_t)(dpg2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; dpg2_fn_t fn; } dpg2_dispatch_t;
static const dpg2_dispatch_t DPG2_DISPATCH[] = {
    { "_route_key",                       dpg2_route_key_fn },
    { "_caches_prompts",                  dpg2_caches_prompts_fn },
    { "_first_response_started",          dpg2_first_response_started_fn },
    { "_stop_requested",                  dpg2_stop_requested_fn },
    { "PrefixCacheGate.__init__",         dpg2_gate_init_fn },
    { "PrefixCacheGate.for_children",     dpg2_gate_for_children },
    { "PrefixCacheGate.follows",          dpg2_gate_follows_fn },
    { "PrefixCacheGate.run",              dpg2_gate_run },
    { "PrefixCacheGate._wait",            dpg2_gate_wait_fn },
};
#define DPG2_DISPATCH_N (sizeof(DPG2_DISPATCH) / sizeof(DPG2_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *dpg2_h_readline(FILE *f)
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

static void dpg2_h_run(const char *func, const json_t *in)
{
    json_t *st = json_object();
    const json_t *sj = dpg2_get(in, "state");
    if (sj && sj->type == JSON_OBJECT) { json_free(st); st = json_copy(sj); }

    dpg2_script_t sc;
    sc.io = dpg2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    dpg2_io_t io;
    io.call = dpg2_h_dispatch;
    io.ctx = &sc;

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < DPG2_DISPATCH_N; i++) {
        if (strcmp(DPG2_DISPATCH[i].func, func) == 0) {
            ret = DPG2_DISPATCH[i].fn(&io, in, st); found = 1; break;
        }
    }
    if (!found) ret = dpg2_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret);
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st);
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
    while ((line = dpg2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = dpg2_str(dpg2_get(rec, "func"));
        const json_t *in = dpg2_get(rec, "in");
        if (func && in) dpg2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
