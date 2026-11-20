/* t_port_tool_guardrails_flow3.c -- harness for the FLOW port of the
 * UNCLAIMED remainder of agent/tool_guardrails.py in
 * src/agent/tool_guardrails_flow3.c (tg3_* entries).
 *
 * Reads JSON LINES from stdin (or argv[1]):
 *   {"func": <live_name>, "in": {...inputs..., "self": {...}, "io": {...}}}
 * Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state carrier
 * (initialized from in["self"]; {} for pure entries).
 *
 * Script semantics (shared with the oracle Script):
 *   absent key / exhausted queue -> fail-closed RuntimeError "no script for X"
 *   non-list value               -> constant result reused every call
 *   list value                   -> per-call queue (one result per call)
 *   {"__raise__":K,"message":M}  -> raised exception of kind K
 *   {"__script__":true,"ret":X}  -> list-valued result X (queue-ambiguity wrap)
 *   null                         -> None result (delivered as NULL pointer)
 */
#include "agent/tool_guardrails_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* drop None-valued object keys recursively (matches oracle norm_args). */
static json_t *tg3_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, tg3_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy((json_t *)v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], tg3_h_strip_nulls(val));
    }
    return o;
}

typedef struct {
    json_t *io;
    json_t *cursors;
    json_t *trace;
} tg3_script_t;

static json_t *tg3_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    tg3_script_t *sc = (tg3_script_t *)ctx;
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
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = tg3_xfmt("no script for %s", name);
        retnode = tg3_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", tg3_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

static char *tg3_h_readline(FILE *f)
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

static void tg3_h_run(const char *func, json_t *in)
{
    tg3_script_t sc;
    sc.io = tg3_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    tg3_io_t io;
    io.call = tg3_h_dispatch;
    io.ctx = &sc;

    json_t *st = tg3_get(in, "self");
    st = st ? json_copy(st) : json_object();
    if (!tg3_obj(st)) { json_free(st); st = json_object(); }
    /* fixed state schema on both sides: fill missing keys with defaults so the
     * emitted carrier is identical in shape for pure and stateful entries. */
    if (!tg3_get(st, "config")) json_set(st, "config", json_object());
    if (!tg3_get(st, "_halt_decision")) json_set(st, "_halt_decision", json_null());
    if (!tg3_get(st, "_identical_streak_sig")) json_set(st, "_identical_streak_sig", json_null());
    if (!tg3_get(st, "_identical_streak_result_hash")) json_set(st, "_identical_streak_result_hash", json_string(""));
    if (!tg3_get(st, "_identical_streak_count")) json_set(st, "_identical_streak_count", json_number(0));
    if (!tg3_get(st, "_identical_streak_first_call_id")) json_set(st, "_identical_streak_first_call_id", json_string(""));
    if (!tg3_get(st, "_call_history")) json_set(st, "_call_history", json_array());
    if (!tg3_get(st, "_persisted_result_paths")) json_set(st, "_persisted_result_paths", json_object());

    json_t *in_echo = json_copy(in);
    json_t *ret = tg3_dispatch(func, &io, in, st);
    if (!ret) ret = json_null();

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
    while ((line = tg3_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = tg3_sval(tg3_get(rec, "func"));
        json_t *in = tg3_get(rec, "in");
        if (func && in) tg3_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
