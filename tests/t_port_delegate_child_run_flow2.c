/* t_port_delegate_child_run_flow2.c — oracle harness for the FLOW port of the
 * unclaimed tools/delegate_tool_child_run.py remainder
 * (src/tools/delegate_child_run_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carriers. Emits JSON LINES:
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
 * _checkpointing is a closure factory: after building the cb marker the harness
 * invokes the transcribed _cb body (dcr2_cb_invoke) once per in.events entry; a
 * raise from any invocation becomes the case ret (mirroring the oracle drive).
 *
 * Gate: bash tests/run_one_oracle.sh delegate_child_run_flow2 \
 *           oracle_delegate_child_run_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/delegate_child_run_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name io queue cursors */
    json_t *trace;       /* owned ordered events */
} dcr2_script_t;

/* drop None-valued object keys recursively; array ELEMENTS are kept but dict
 * elements are recursed — matches oracle norm_args. */
static json_t *dcr2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, dcr2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], dcr2_h_strip_nulls(val));
    }
    return o;
}

static json_t *dcr2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    dcr2_script_t *sc = (dcr2_script_t *)ctx;
    json_t *retnode = NULL;
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
    if (failclosed) {
        char *m = dcr2_xfmt("no script for %s", name);
        retnode = dcr2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else retnode = actual ? json_copy(actual) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", dcr2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (dcr2_is_raise(retnode)) return retnode;
    if (!retnode || retnode->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*dcr2_fn_t)(dcr2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; dcr2_fn_t fn; } dcr2_dispatch_t;
static const dcr2_dispatch_t DCR2_DISPATCH[] = {
    { "_reserve_child_transcript", dcr2_reserve_child_transcript },
    { "_format_thread_stack", dcr2_format_thread_stack },
    { "_diag_section", dcr2_diag_section },
    { "_diag_sizes", dcr2_diag_sizes },
    { "_budget_warning_text", dcr2_budget_warning_text_fn },
    { "_warn_child_budget", dcr2_warn_child_budget },
    { "_child_last_event_age", dcr2_child_last_event_age },
    { "_register_child", dcr2_register_child },
    { "_create_isolated_worktree", dcr2_create_isolated_worktree },
    { "_lease_child_credential", dcr2_lease_child_credential },
    { "_merge_late_steer", dcr2_merge_late_steer },
    { "_validate_child_output_schema", dcr2_validate_child_output_schema },
    { "_build_tool_trace", dcr2_build_tool_trace },
    { "_build_result_entry", dcr2_build_result_entry },
    { "_is_image_url", dcr2_is_image_url_fn },
    { "_build_child_goal_message", dcr2_build_child_goal_message },
    { "_checkpointing", dcr2_checkpointing },
};
#define DCR2_DISPATCH_N (sizeof(DCR2_DISPATCH) / sizeof(DCR2_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *dcr2_h_readline(FILE *f)
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

static void dcr2_h_run(const char *func, const json_t *in)
{
    json_t *st = json_object();
    const json_t *sj = dcr2_get(in, "state");
    if (dcr2_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    dcr2_script_t sc;
    sc.io = dcr2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    dcr2_io_t io;
    io.call = dcr2_h_dispatch;
    io.ctx = &sc;

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < DCR2_DISPATCH_N; i++) {
        if (strcmp(DCR2_DISPATCH[i].func, func) == 0) {
            ret = DCR2_DISPATCH[i].fn(&io, in, st);
            found = 1;
            /* _checkpointing: invoke the transcribed _cb body per event. */
            if (strcmp(func, "_checkpointing") == 0 && !dcr2_is_raise(ret)) {
                json_t *events = dcr2_get(in, "events");
                if (dcr2_is_arr(events)) {
                    for (size_t e = 0; e < json_len(events); e++) {
                        json_t *r = dcr2_cb_invoke(&io, in, json_get(events, e));
                        if (dcr2_is_raise(r)) { json_free(ret); ret = r; break; }
                        json_free(r);
                    }
                }
            }
            break;
        }
    }
    if (!found) ret = dcr2_mkraise("RuntimeError", "unknown func");
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
    while ((line = dcr2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = dcr2_str(dcr2_get(rec, "func"));
        const json_t *in = dcr2_get(rec, "in");
        if (func && in) dcr2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}