/* t_port_context_compressor_flow6.c — oracle harness for the sixth-wave FLOW
 * port of agent/context_compressor.py (src/agent/context_compressor_flow6.c).
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
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow6 \
 *           oracle_context_compressor_flow6.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_compressor_flow6.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cc6_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cc6_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cc6_h_strip_nulls(val));
    }
    return o;
}

static json_t *cc6_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cc6_script_t *sc = (cc6_script_t *)ctx;
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
        char *m = cc6_xfmt("no script for %s", name);
        retnode = cc6_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cc6_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cc6_fn_t)(cc6_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cc6_fn_t fn; } cc6_dispatch_t;
static const cc6_dispatch_t CC6_DISPATCH[] = {
    { "_is_real_user_turn", cc6_is_real_user_turn },
    { "_real_user_indices_desc", cc6_real_user_indices_desc },
    { "preview_threshold_tokens", cc6_preview_threshold_tokens },
    { "_render_tool_call_for_summary", cc6_render_tool_call_for_summary },
    { "_drop_blank_echoes", cc6_drop_blank_echoes },
    { "_prune_boundary", cc6_prune_boundary },
    { "_demote_stale_tail_tools", cc6_demote_stale_tail_tools },
    { "_clear_terminal_summary_failures", cc6_clear_terminal_summary_failures },
    { "maybe_seed_preflight_display_tokens", cc6_maybe_seed_preflight_display_tokens },
    { "note_native_compaction_checkpoint", cc6_note_native_compaction_checkpoint },
    { "record_rejected_compaction", cc6_record_rejected_compaction },
    { "_apply_real_prompt_verdict", cc6_apply_real_prompt_verdict },
    { "_abort_on_summary_failure", cc6_abort_on_summary_failure },
    { "_assemble_head", cc6_assemble_head },
    { "_assemble_tail", cc6_assemble_tail },
    { "_dedupe_tool_results", cc6_dedupe_tool_results },
    { "_augment_summary_lean", cc6_augment_summary_lean },
    { "_merge_summary_into_tail_row", cc6_merge_summary_into_tail_row },
};
#define CC6_DISPATCH_N (sizeof(CC6_DISPATCH) / sizeof(CC6_DISPATCH[0]))

static const char *CC6_MUTABLE[] = { "self", "msg", "result", "telemetry", NULL };

/* ---- line reader (\n only) ---- */
static char *cc6_h_readline(FILE *f)
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

static void cc6_h_run(const char *func, const json_t *in)
{
    cc6_script_t sc;
    sc.io = cc6_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cc6_io_t io;
    io.call = cc6_h_dispatch;
    io.ctx = &sc;

    /* seed mutable state from in (any present key, mirroring the oracle) */
    json_t *st = json_object();
    for (int k = 0; CC6_MUTABLE[k]; k++) {
        const json_t *mv = cc6_get(in, CC6_MUTABLE[k]);
        if (mv) json_set(st, CC6_MUTABLE[k], json_copy(mv));
    }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CC6_DISPATCH_N; i++) {
        if (strcmp(CC6_DISPATCH[i].func, func) == 0) { ret = CC6_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cc6_mkraise("RuntimeError", "unknown func");
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
    while ((line = cc6_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cc6_str(cc6_get(rec, "func"));
        const json_t *in = cc6_get(rec, "in");
        if (func && in) cc6_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
