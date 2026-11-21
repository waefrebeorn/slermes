/* t_port_context_compressor_flow8.c — oracle harness for the eighth-wave FLOW
 * port of agent/context_compressor.py (src/agent/context_compressor_flow8.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts. Emits JSON LINES:
 * {"func","in","ret","trace","st"}.
 *
 * Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
 * (recorded as the event ret BEFORE propagation). A bare JSON array script is
 * a per-call QUEUE; a list RESULT must be wrapped {"__script__":true,"ret":[...]}.
 * A scripted null returns a NULL pointer (None).
 *
 * MUTABLE_KEYS (self, msg, result, telemetry) are seeded into `st` from `in`
 * so ported methods can mutate state in place; `st` is emitted for comparison.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow8 \
 *           oracle_context_compressor_flow8.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_compressor_flow8.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cc8_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cc8_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cc8_h_strip_nulls(val));
    }
    return o;
}

static json_t *cc8_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cc8_script_t *sc = (cc8_script_t *)ctx;
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
        char *m = cc8_xfmt("no script for %s", name);
        retnode = cc8_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cc8_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cc8_fn_t)(cc8_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cc8_fn_t fn; } cc8_dispatch_t;
static const cc8_dispatch_t CC8_DISPATCH[] = {
    { "_reset_micro_compact_cursor_state", cc8_reset_micro_compact_cursor_state },
    { "_reset_real_usage_pairing", cc8_reset_real_usage_pairing },
    { "_reset_consecutive_overload_aborts", cc8_reset_consecutive_overload_aborts },
    { "_reset_proactive_prune_rearm", cc8_reset_proactive_prune_rearm },
    { "_temporal_anchoring_rule", cc8_temporal_anchoring_rule },
    { "_structural_no_op_result", cc8_structural_no_op_result },
    { "_warn_reclamation_no_op", cc8_warn_reclamation_no_op },
    { "_spared_pending_tool_round", cc8_spared_pending_tool_round },
    { "_demote_tool_result_at", cc8_demote_tool_result_at },
    { "_serialize_records_for_summary", cc8_serialize_records_for_summary },
    { "_summary_template_sections", cc8_summary_template_sections },
    { "_build_summary_prompt", cc8_build_summary_prompt },
};
#define CC8_DISPATCH_N (sizeof(CC8_DISPATCH) / sizeof(CC8_DISPATCH[0]))

static const char *CC8_MUTABLE[] = { "self", "msg", "result", "telemetry", NULL };

/* ---- line reader (\n only) ---- */
static char *cc8_h_readline(FILE *f)
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

static void cc8_h_run(const char *func, const json_t *in)
{
    cc8_script_t sc;
    sc.io = cc8_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cc8_io_t io;
    io.call = cc8_h_dispatch;
    io.ctx = &sc;

    /* seed mutable state from in (any present key, mirroring the oracle) */
    json_t *st = json_object();
    for (int k = 0; CC8_MUTABLE[k]; k++) {
        const json_t *mv = cc8_get(in, CC8_MUTABLE[k]);
        if (mv) json_set(st, CC8_MUTABLE[k], json_copy(mv));
    }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CC8_DISPATCH_N; i++) {
        if (strcmp(CC8_DISPATCH[i].func, func) == 0) { ret = CC8_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cc8_mkraise("RuntimeError", "unknown func");
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
    while ((line = cc8_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cc8_str(cc8_get(rec, "func"));
        const json_t *in = cc8_get(rec, "in");
        if (func && in) cc8_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
