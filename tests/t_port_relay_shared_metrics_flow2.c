/* t_port_relay_shared_metrics_flow2.c — oracle harness for the UNCLAIMED-remainder
 * FLOW + leaf port of hermes_cli/observability/relay_shared_metrics.py
 * (src/cli/relay_shared_metrics_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carrier. Emits JSON LINES:
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
 * Gate: bash tests/run_one_oracle.sh relay_shared_metrics_flow2 \
 *           oracle_relay_shared_metrics_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/relay_shared_metrics_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} rsm2_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *rsm2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], rsm2_h_strip_nulls(val));
    }
    return o;
}

static json_t *rsm2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    rsm2_script_t *sc = (rsm2_script_t *)ctx;
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
        char *m = rsm2_xfmt("no script for %s", name);
        retnode = rsm2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", rsm2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*rsm2_fn_t)(rsm2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; rsm2_fn_t fn; } rsm2_dispatch_t;
static const rsm2_dispatch_t RSM2_DISPATCH[] = {
    { "_text", rsm2_text_field },
    { "_elapsed_ms", rsm2_elapsed_ms },
    { "_scope_handle", rsm2_scope_handle },
    { "_sole", rsm2_sole },
    { "_forget", rsm2_forget },
    { "_task_parent_handle", rsm2_task_parent_handle },
    { "_with_launch_entrypoint", rsm2_with_launch_entrypoint },
    { "_cold_key", rsm2_cold_key },
    { "_process_marks_admitted", rsm2_process_marks_admitted },
    { "_raw_config", rsm2_raw_config },
    { "_resolved_send_config", rsm2_resolved_send_config },
    { "_guarded", rsm2_guarded },
    { "_with_scope_stack", rsm2_with_scope_stack },
    { "_emit_rows", rsm2_emit_rows },
    { "_any_session", rsm2_any_session },
    { "_task_pair", rsm2_task_pair },
    { "_abort_session", rsm2_abort_session },
    { "start_user_turn", rsm2_start_user_turn },
    { "_deregister", rsm2_deregister },
    { "_join_send_thread", rsm2_join_send_thread },
    { "_release", rsm2_release },
    { "_lineage_spent", rsm2_lineage_spent },
    { "_close_child_session", rsm2_close_child_session },
    { "_run_task_hook", rsm2_run_task_hook },
    { "record_process_mark", rsm2_record_process_mark },
    { "record_process_marks_saved", rsm2_record_process_marks_saved },
    { "close_session_run", rsm2_close_session_run },
    { "rotate_segment", rsm2_rotate_segment },
    { "shutdown_runtimes", rsm2_shutdown_runtimes },
};
#define RSM2_DISPATCH_N (sizeof(RSM2_DISPATCH) / sizeof(RSM2_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *rsm2_h_readline(FILE *f)
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

static void rsm2_h_run(const char *func, const json_t *in)
{
    rsm2_script_t sc;
    sc.io = rsm2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    rsm2_io_t io;
    io.call = rsm2_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = rsm2_get(in, "state");
    if (rsm2_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < RSM2_DISPATCH_N; i++) {
        if (strcmp(RSM2_DISPATCH[i].func, func) == 0) { ret = RSM2_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = rsm2_mkraise("RuntimeError", "unknown func");
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
    while ((line = rsm2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = rsm2_str(rsm2_get(rec, "func"));
        const json_t *in = rsm2_get(rec, "in");
        if (func && in) rsm2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}