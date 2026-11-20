/* t_port_context_budget_flow.c — oracle harness for the context-window budget
 * FLOW + leaf port (src/agent/context_budget_flow.c; real defining module
 * agent/context_compressor_window.py + leaves).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the CompactionWindowMixin instance
 * attributes. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window
 * (no ported def writes a self attribute, so st echoes in.state).
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh context_budget_flow \
 *           oracle_context_budget_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_budget_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cbg_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cbg_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cbg_h_strip_nulls(val));
    }
    return o;
}

static json_t *cbg_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cbg_script_t *sc = (cbg_script_t *)ctx;
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
        char *m = cbg_xfmt("no script for %s", name);
        retnode = cbg_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cbg_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cbg_fn_t)(cbg_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cbg_fn_t fn; } cbg_dispatch_t;
static const cbg_dispatch_t CBG_DISPATCH[] = {
    { "_restart_probe_extra", cbg_restart_probe_extra },
    { "summary_view", cbg_summary_view },
    { "_compress_window", cbg_compress_window_entry },
    { "_plan_compaction_window", cbg_plan_compaction_window },
    { "_commit_window", cbg_commit_window },
    { "_assemble_with_archive_list", cbg_assemble_with_archive_list },
    { "_offload_window_results", cbg_offload_window_results_entry },
    { "_take_prepared_summary", cbg_take_prepared_summary },
    { "_adopt_prepared_summary", cbg_adopt_prepared_summary },
    { "tail_cut_progress_floor", cbg_tail_cut_progress_floor_entry },
    { "_tool_calls_by_id", cbg_tool_calls_by_id_entry },
    { "_tc_get", cbg_tc_get },
    { "_rewritten", cbg_rewritten },
    { "archive_refs_in_text", cbg_archive_refs_in_text },
};
#define CBG_DISPATCH_N (sizeof(CBG_DISPATCH) / sizeof(CBG_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *cbg_h_readline(FILE *f)
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

static void cbg_h_run(const char *func, const json_t *in)
{
    cbg_script_t sc;
    sc.io = cbg_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cbg_io_t io;
    io.call = cbg_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = cbg_get(in, "state");
    if (cbg_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CBG_DISPATCH_N; i++) {
        if (strcmp(CBG_DISPATCH[i].func, func) == 0) { ret = CBG_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cbg_mkraise("RuntimeError", "unknown func");
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
    while ((line = cbg_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cbg_str(cbg_get(rec, "func"));
        const json_t *in = cbg_get(rec, "in");
        if (func && in) cbg_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}