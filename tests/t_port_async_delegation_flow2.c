/* t_port_async_delegation_flow2.c — oracle harness for the FLOW port of the
 * unclaimed async_delegation.py remainder (src/tools/async_delegation_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * in.io carries universal-seam scripts (seam-name -> constant or per-call
 * queue) and in.state carries the mutable module-state window. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 *
 * Fail-closed: absent / exhausted script returns a RuntimeError raise spec,
 * recorded as the event ret BEFORE propagation. A bare JSON array script is a
 * per-call QUEUE; a list RESULT must be wrapped {"__script__":true,"ret":[...]}.
 * A scripted null returns a NULL pointer.
 *
 * Gate: bash tests/run_one_oracle.sh async_delegation_flow2 \
 *           oracle_async_delegation_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/async_delegation_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} ad2_script_t;

static json_t *ad2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, ad2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], ad2_h_strip_nulls(val));
    }
    return o;
}

static json_t *ad2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    ad2_script_t *sc = (ad2_script_t *)ctx;
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
        char *m = ad2_xfmt("no script for %s", name);
        retnode = ad2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else retnode = actual ? json_copy(actual) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", ad2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (ad2_is_raise(retnode)) return retnode;
    if (!retnode || retnode->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*ad2_fn_t)(ad2_io_t *, const json_t *, json_t *);
typedef struct { const char *func; ad2_fn_t fn; } ad2_dispatch_t;

static json_t *ad2_unfence_entry(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    ad2_unfence(st, ad2_str(ad2_get(in, "token")));
    return json_null();
}

static const ad2_dispatch_t AD2_DISPATCH[] = {
    { "_single_crash", ad2_single_crash },
    { "_batch_crash", ad2_batch_crash },
    { "_batch_status", ad2_batch_status },
    { "_stalled_error_text", ad2_stalled_error_text },
    { "_stall_meta", ad2_stall_meta },
    { "_stalled_result", ad2_stalled_result },
    { "_keep_finished_children", ad2_keep_finished_children },
    { "_recovered_results", ad2_recovered_results },
    { "is_interim_delegation_event", ad2_is_interim_entry },
    { "completion_copy_id", ad2_completion_copy_id },
    { "_call_interrupt", ad2_call_interrupt },
    { "mark_copy_held", ad2_mark_copy_held },
    { "unmark_copy_held", ad2_unmark_copy_held },
    { "refused_for_session", ad2_refused_for_session },
    { "allow_for_session", ad2_allow_for_session },
    { "_unfence", ad2_unfence_entry },
    { "refuse_for_session", ad2_refuse_for_session },
    { "_session_records", ad2_session_records },
    { "_queued_completion_ids", ad2_queued_completion_ids },
    { "_copy_may_be_live", ad2_copy_may_be_live },
    { "_note_reoffers", ad2_note_reoffers },
    { "maybe_sweep_orphaned_completions", ad2_maybe_sweep_orphaned_completions },
    { "_event_delivery", ad2_event_delivery },
    { "return_completion_offer", ad2_return_completion_offer },
};
#define AD2_DISPATCH_N (sizeof(AD2_DISPATCH) / sizeof(AD2_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *ad2_h_readline(FILE *f)
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

static void ad2_h_run(const char *func, const json_t *in)
{
    json_t *st = json_object();
    const json_t *sj = ad2_get(in, "state");
    if (ad2_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    ad2_script_t sc;
    sc.io = ad2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();

    ad2_io_t io;
    io.call = ad2_h_dispatch;
    io.ctx = &sc;

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < AD2_DISPATCH_N; i++) {
        if (strcmp(AD2_DISPATCH[i].func, func) == 0) { ret = AD2_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = ad2_mkraise("RuntimeError", "unknown func");
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
    while ((line = ad2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = ad2_str(ad2_get(rec, "func"));
        const json_t *in = ad2_get(rec, "in");
        if (func && in) ad2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
