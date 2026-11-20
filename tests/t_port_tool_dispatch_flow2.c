/* t_port_tool_dispatch_flow2.c — oracle harness for the FLOW port of the
 * unclaimed delegate_task dispatch remainder (src/agent/tool_dispatch_flow2.c).
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
 * active_children_op is a DETERMINISTIC modeled seam (like flow1's future
 * registry): it mutates the parent's _active_children array (append / remove
 * by __id__; remove-absent raises ValueError "list.remove(x): x not in list").
 * The __children__ fixture marker is converted to a plain array at setup.
 *
 * Gate: bash tests/run_one_oracle.sh tool_dispatch_flow2 \
 *           oracle_tool_dispatch_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/tool_dispatch_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name io queue cursors */
    json_t *trace;       /* owned ordered events */
    json_t *st;          /* borrowed state window (active_children_op mutates) */
} td2_script_t;

/* drop None-valued object keys recursively; array ELEMENTS are kept but dict
 * elements are recursed — matches oracle norm_args. */
static json_t *td2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, td2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], td2_h_strip_nulls(val));
    }
    return o;
}

/* the parent's _active_children array (after __children__ conversion). */
static json_t *td2_h_children_array(td2_script_t *sc)
{
    json_t *parent = td2_get(sc->st, "parent_agent");
    json_t *ac = td2_dattr(parent, "_active_children");
    return td2_is_arr(ac) ? ac : NULL;
}

/* deterministic active_children_op: append / remove by __id__. */
static json_t *td2_h_children_op(td2_script_t *sc, const json_t *args)
{
    const char *op = td2_str(td2_get(args, "op"));
    json_t *child = td2_get(args, "child");
    json_t *arr = td2_h_children_array(sc);
    if (!arr) return td2_mkraise("RuntimeError", "no _active_children array");
    if (op && strcmp(op, "append") == 0) {
        json_append(arr, child ? json_copy(child) : json_null());
        return json_null();
    }
    if (op && strcmp(op, "remove") == 0) {
        const char *cid = td2_str(td2_get(child, "__id__"));
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *c = json_get(arr, i);
            const char *iid = td2_str(td2_get(c, "__id__"));
            int match = (cid && iid) ? strcmp(cid, iid) == 0 : (c == child);
            if (match) { json_array_remove(arr, i); return json_null(); }
        }
        return td2_mkraise("ValueError", "list.remove(x): x not in list");
    }
    return td2_mkraise("RuntimeError", "bad op");
}

static json_t *td2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    td2_script_t *sc = (td2_script_t *)ctx;
    json_t *retnode = NULL;
    if (strcmp(name, "active_children_op") == 0) {
        retnode = td2_h_children_op(sc, args);
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
            char *m = td2_xfmt("no script for %s", name);
            retnode = td2_mkraise("RuntimeError", m ? m : "no script");
            free(m);
        } else retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", td2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (td2_is_raise(retnode)) return retnode;
    if (!retnode || retnode->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*td2_fn_t)(td2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; td2_fn_t fn; } td2_dispatch_t;
static const td2_dispatch_t TD2_DISPATCH[] = {
    { "_num", td2_num_fn },
    { "_str_or_none", td2_str_or_none_fn },
    { "_tier_in_effect", td2_tier_in_effect },
    { "_route_report", td2_route_report },
    { "_fabricated_entry", td2_fabricated_entry },
    { "_append_missed_steer", td2_append_missed_steer },
    { "_close_child", td2_close_child },
    { "_with_children_lock", td2_with_children_lock },
    { "_attach_child", td2_attach_child },
    { "_detach_child", td2_detach_child },
    { "_signal_child_stop", td2_signal_child_stop },
    { "_child_activity_fingerprint", td2_child_activity_fingerprint },
    { "_quiet", td2_quiet_fn },
    { "_safe_progress", td2_safe_progress },
    { "describe_subagent_failure", td2_describe_subagent_failure },
    { "_format_duration", td2_format_duration },
    { "_print_completion_line", td2_print_completion_line },
    { "_short", td2_short_fn },
    { "_route_key", td2_route_key },
    { "_caches_prompts", td2_caches_prompts },
    { "_first_response_started", td2_first_response_started },
    { "_stop_requested", td2_stop_requested },
    { "PrefixCacheGate.for_children", td2_gate_for_children },
    { "PrefixCacheGate.run", td2_gate_run },
    { "_get_prefix_stagger_seconds", td2_get_prefix_stagger_seconds },
    { "waiting_on_resume", td2_waiting_on_resume },
    { "finite_turn_required", td2_finite_turn_required },
    { "_process_accounting_lines", td2_process_accounting_lines },
};
#define TD2_DISPATCH_N (sizeof(TD2_DISPATCH) / sizeof(TD2_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *td2_h_readline(FILE *f)
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

/* convert a {"__children__":[...]} marker to a plain array in place. */
static void td2_h_conv_children(json_t *node)
{
    if (!td2_is_obj(node)) return;
    json_t *ac = td2_dattr(node, "_active_children");
    if (td2_is_obj(ac)) {
        json_t *kids = td2_get(ac, "__children__");
        if (td2_is_arr(kids)) {
            json_t *arr = json_copy(kids);
            td2_dset(node, "_active_children", arr);
        }
    }
}

/* Live list.append(child) stores a REFERENCE to the same object as st.child, so
 * a later child mutation (e.g. _signal_child_stop's setattr) is visible in the
 * list member. C trees cannot alias: synchronize any _active_children member
 * whose __id__ matches st.child to the current st.child before the state echo. */
static void td2_h_sync_children(json_t *st)
{
    json_t *child = td2_get(st, "child");
    const char *cid = td2_str(td2_get(child, "__id__"));
    if (!cid) return;
    json_t *parent = td2_get(st, "parent_agent");
    json_t *arr = td2_dattr(parent, "_active_children");
    if (!td2_is_arr(arr)) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *c = json_get(arr, i);
        const char *iid = td2_str(td2_get(c, "__id__"));
        if (iid && strcmp(iid, cid) == 0) json_array_set(arr, i, json_copy(child));
    }
}

static void td2_h_run(const char *func, const json_t *in)
{
    json_t *st = json_object();
    const json_t *sj = td2_get(in, "state");
    if (td2_is_obj(sj)) { json_free(st); st = json_copy(sj); }
    td2_h_conv_children(td2_get(st, "parent_agent"));

    td2_script_t sc;
    sc.io = td2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    sc.st = st;
    td2_io_t io;
    io.call = td2_h_dispatch;
    io.ctx = &sc;

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < TD2_DISPATCH_N; i++) {
        if (strcmp(TD2_DISPATCH[i].func, func) == 0) { ret = TD2_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = td2_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    td2_h_sync_children(st);
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
    while ((line = td2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = td2_str(td2_get(rec, "func"));
        const json_t *in = td2_get(rec, "in");
        if (func && in) td2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}