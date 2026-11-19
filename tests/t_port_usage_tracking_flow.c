/* t_port_usage_tracking_flow.c -- harness for the FLOW + leaf port of the
 * /usage account-usage tracking cluster (agent/account_usage.py +
 * agent/account_usage_nous.py) in src/agent/usage_tracking_flow.c.
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection
 * ({} always here -- every ported function is pure).
 *
 * The whole port .c is #included (area-qualified); the gate links libjson
 * alone. Script semantics (shared with the oracle Script):
 *   absent key / exhausted queue -> fail-closed RuntimeError "no script for X"
 *   non-list value               -> constant result reused every call
 *   list value                   -> per-call queue (one result per call)
 *   {"__raise__":K,"message":M}  -> raised exception of kind K
 *   {"__script__":true,"ret":X}  -> list-valued result X (queue-ambiguity wrap)
 *   null                         -> None result (delivered as NULL pointer)
 */
#include "agent/usage_tracking_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} ut_script_t;

/* drop None-valued object keys recursively; recurse into arrays so dict
 * elements are stripped too (matches the oracle norm_args). Null array
 * elements are kept. */
static json_t *ut_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, ut_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], ut_h_strip_nulls(val));
    }
    return o;
}

static json_t *ut_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    ut_script_t *sc = (ut_script_t *)ctx;
    json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    json_t *result = NULL;
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
    /* unwrap {"__script__":true,"ret":...} list-result wrapper */
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = ut_xfmt("no script for %s", name);
        retnode = ut_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", ut_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ----
 * Cases carry the LIVE name (e.g. "_codex_reset_guard"); the C entry suffix
 * drops the leading underscore. _is_num maps to ut_is_num_entry (the ut_is_num
 * name is the static isinstance helper). */
static json_t *ut_h_entry(const char *func, ut_io_t *io, const json_t *in, json_t *st)
{
    if (func[0] == '_') func++;
    if (strcmp(func, "is_num") == 0) return ut_is_num_entry(io, in, st);
#define UT_DISPATCH(nm) if (strcmp(func, #nm) == 0) return ut_##nm(io, in, st)
    UT_DISPATCH(codex_banked_resets);
    UT_DISPATCH(codex_window_labels);
    UT_DISPATCH(codex_headers);
    UT_DISPATCH(codex_snapshot_identity);
    UT_DISPATCH(unavailable);
    UT_DISPATCH(codex_reset_guard);
    UT_DISPATCH(codex_reset_outcome);
    UT_DISPATCH(snapshot);
    UT_DISPATCH(usage_windows);
    UT_DISPATCH(account_limit_lines);
    UT_DISPATCH(reset_phrase);
    UT_DISPATCH(render_usage_window_lines);
    UT_DISPATCH(call_plugin_usage_hook);
    UT_DISPATCH(fetch_codex_account_usage_impl);
    UT_DISPATCH(fetch_codex_account_usage_read_only);
    UT_DISPATCH(header_gauge_percent);
    UT_DISPATCH(bp_window);
    UT_DISPATCH(window_top_tier_line);
    UT_DISPATCH(nous_snapshot);
    UT_DISPATCH(usage_window_rows);
    UT_DISPATCH(apply_header_gauge);
    UT_DISPATCH(nous_logged_in);
    UT_DISPATCH(nous_account_view);
    UT_DISPATCH(nous_credits_view);
    UT_DISPATCH(nous_credits_usage_fields);
    UT_DISPATCH(nous_usage_lines);
    UT_DISPATCH(nous_usage_view);
#undef UT_DISPATCH
    return ut_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only; fixtures may carry raw U+2028 inside strings) ---- */
static char *ut_h_readline(FILE *f)
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

static void ut_h_run(const char *func, json_t *in)
{
    ut_script_t sc;
    sc.io = ut_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    ut_io_t io;
    io.call = ut_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = ut_h_entry(func, &io, in, st);
    /* every ported function is pure: st stays {} on success and raise alike */
    if (ut_is_raise(ret)) {
        json_free(st);
        st = json_object();
    }
    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret ? ret : json_null());
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
    while ((line = ut_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = ut_str(ut_get(rec, "func"));
        json_t *in = ut_get(rec, "in");
        if (func && in) ut_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}