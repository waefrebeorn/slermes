/* t_port_tool_dispatch_flow.c — oracle harness for the FLOW port of
 * tools/delegate_tool_dispatch.py (src/agent/tool_dispatch_flow.c).
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
 * Futures: executor.submit registers its result spec {"id","done","result",
 * "cancelled"} in a per-id registry; future.done / future.result /
 * future.cancelled consume the per-item script ON the spec (single value
 * reused, bare array = per-call queue) and record args {"id"}. cf_wait returns
 * the scripted [done_ids, pending_ids] which the port resolves by id.
 *
 * Gate: bash tests/run_one_oracle.sh tool_dispatch_flow \
 *           oracle_tool_dispatch_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/tool_dispatch_flow.c"

/* ---- universal-seam recording dispatcher + future registry ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name io queue cursors */
    json_t *trace;       /* owned ordered events */
    json_t *futures;     /* owned {id: {spec, cursors}} registry */
} td_script_t;

/* drop None-valued object keys recursively; array ELEMENTS are kept (a None
 * element stays) but dict elements are recursed — matches oracle norm_args. */
static json_t *td_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, td_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], td_h_strip_nulls(val));
    }
    return o;
}

/* consume a per-item future script field; returns a NEW value node or a raise
 * spec (fail-closed). Mirrors oracle FutureDouble._item. */
static json_t *td_h_future_item(td_script_t *sc, const char *id, const char *field, const char *seam)
{
    json_t *reg = id ? json_obj_get(sc->futures, id) : NULL;
    json_t *spec = reg ? json_obj_get(reg, "spec") : NULL;
    const json_t *entry = spec ? json_obj_get(spec, field) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    const json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            json_t *cur = json_obj_get(reg, "cursors");
            double c = json_get_num(cur, field, 0);
            size_t idx = (size_t)c;
            if (idx >= json_len(entry)) exhausted = 1;
            else { result = json_get(entry, idx); json_set(cur, field, json_number(c + 1)); }
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
        char *m = td_xfmt("no script for %s", seam);
        retnode = td_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else retnode = actual ? json_copy(actual) : json_null();
    return retnode;
}

static json_t *td_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    td_script_t *sc = (td_script_t *)ctx;
    json_t *retnode = NULL;
    int is_future = (strncmp(name, "future.", 7) == 0);
    if (is_future) {
        const char *id = td_str(td_get(args, "id"));
        const char *field = name + 7;   /* done / result / cancelled */
        retnode = td_h_future_item(sc, id, field, name);
    } else {
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
            char *m = td_xfmt("no script for %s", name);
            retnode = td_mkraise("RuntimeError", m ? m : "no script");
            free(m);
        } else retnode = actual ? json_copy(actual) : json_null();
        /* register a submitted future spec by id */
        if (!failclosed && strcmp(name, "executor.submit") == 0 && retnode && retnode->type == JSON_OBJECT) {
            const char *fid = td_str(td_get(retnode, "id"));
            if (fid) {
                json_t *reg = json_object();
                json_set(reg, "spec", json_copy(retnode));
                json_set(reg, "cursors", json_object());
                json_set(sc->futures, fid, reg);
            }
        }
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", td_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (td_is_raise(retnode)) return retnode;
    if (!retnode || retnode->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*td_fn_t)(td_io_t *, const json_t *, json_t *);
typedef struct { const char *func; td_fn_t fn; } td_dispatch_t;
static const td_dispatch_t TD_DISPATCH[] = {
    { "_Batch.owner_kwargs", td_owner_kwargs },
    { "_Batch.run_child", td_run_child },
    { "_announce_batch", td_announce_batch },
    { "_capture_origin", td_capture_origin },
    { "_report_child_done", td_report_child_done },
    { "_record_finished_child", td_record_finished_child },
    { "_waiting_on_resume", td_waiting_on_resume },
    { "_release_unstarted_child", td_release_unstarted_child },
    { "_run_gated", td_run_gated },
    { "_run_children_parallel", td_run_children_parallel },
    { "_execute_and_aggregate", td_execute_and_aggregate },
    { "_run_sync_with_note", td_run_sync_with_note },
    { "_resolve_async_wake_sid", td_resolve_async_wake_sid },
    { "_resolve_async_session_key", td_resolve_async_session_key },
    { "_batch_progress_token", td_batch_progress_token },
    { "_dispatched_payload", td_dispatched_payload },
    { "_units_of", td_units_of },
    { "_dispatch_unit", td_dispatch_unit },
    { "_restore_parent_cancellation", td_restore_parent_cancellation },
    { "_dispatch_background", td_dispatch_background },
    { "_refuse_detached_only", td_refuse_detached_only },
};
#define TD_DISPATCH_N (sizeof(TD_DISPATCH) / sizeof(TD_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *td_h_readline(FILE *f)
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

/* materialize the carriers each function echoes into its state window */
static void td_h_setup(const char *func, const json_t *in, json_t *st)
{
    json_t *b = td_get(st, "batch");
    if (b) td_batch_defaults(b);
    json_t *u = td_get(st, "unit");
    if (u) td_batch_defaults(u);
    if (strcmp(func, "_Batch.run_child") == 0 || strcmp(func, "_release_unstarted_child") == 0) {
        json_set(st, "child", json_copy(td_get(in, "child")));
    } else if (strcmp(func, "_report_child_done") == 0) {
        json_t *sp = td_get(in, "spinner");
        json_set(st, "spinner", sp ? json_copy(sp) : json_null());
    } else if (strcmp(func, "_run_gated") == 0) {
        json_set(st, "gate", json_copy(td_get(in, "gate")));
        json_set(st, "child", json_copy(td_get(in, "child")));
    } else if (strcmp(func, "_run_children_parallel") == 0) {
        if (!td_is_arr(td_get(st, "results"))) json_set(st, "results", json_array());
    } else if (strcmp(func, "_batch_progress_token") == 0) {
        json_t *ca = td_get(in, "child_agents");
        json_set(st, "child_agents", ca ? json_copy(ca) : json_array());
    }
}

static void td_h_run(const char *func, const json_t *in)
{
    td_script_t sc;
    sc.io = td_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    sc.futures = json_object();
    td_io_t io;
    io.call = td_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = td_get(in, "state");
    if (td_is_obj(sj)) { json_free(st); st = json_copy(sj); }
    td_h_setup(func, in, st);

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < TD_DISPATCH_N; i++) {
        if (strcmp(TD_DISPATCH[i].func, func) == 0) { ret = TD_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = td_mkraise("RuntimeError", "unknown func");
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
    json_free(sc.futures);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = td_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = td_str(td_get(rec, "func"));
        const json_t *in = td_get(rec, "in");
        if (func && in) td_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}