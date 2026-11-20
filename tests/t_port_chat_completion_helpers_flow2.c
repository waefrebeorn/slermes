/* t_port_chat_completion_helpers_flow2.c -- harness for the flow2 remainder of
 * agent/chat_completion_helpers.py (the _StreamingCall shared small helpers).
 *
 * Reads one JSON object per line from stdin (or argv[1]):
 *   {"func": <live_name>, "in": {...}}
 * Emits one JSON object per line:
 *   {"func", "in", "ret", "trace", "st"}
 * Dumb emitter: no Python, no expected values. The oracle diffs against the
 * live CPython drive of the same defs seeded with the same seam scripts.
 */
#include "agent/chat_completion_helpers_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} cch2_script_t;

/* drop None-valued OBJECT keys recursively (array elements kept verbatim) */
static json_t *cch2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy((json_t *)v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cch2_h_strip_nulls(val));
    }
    return o;
}

static json_t *cch2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cch2_script_t *sc = (cch2_script_t *)ctx;
    json_t *entry = sc->io ? json_obj_get(sc->io, (char *)name) : NULL;
    int present = entry != NULL, exhausted = 0;
    json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, (char *)name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else { result = json_get(entry, idx); json_set(sc->cursors, (char *)name, json_number(cur + 1)); }
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
        char *m = cch2_xfmt("no script for %s", name);
        retnode = cch2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cch2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- dispatch table ---- */
static json_t *cch2_h_entry(const char *func, cch2_io_t *io, const json_t *in, json_t *st)
{
    if (func[0] == '_') func++;
#define CCH2F2_DISPATCH(nm) if (strcmp(func, #nm) == 0) return cch2_##nm(io, in, st)
    CCH2F2_DISPATCH(live_route);
    CCH2F2_DISPATCH(visible_text_delivered);
    CCH2F2_DISPATCH(route_switched_under_request);
    CCH2F2_DISPATCH(set_managed_stream);
    CCH2F2_DISPATCH(start_stream_attempt);
    CCH2F2_DISPATCH(cancel_current_stream_attempt);
    CCH2F2_DISPATCH(stream_attempt_is_active);
    CCH2F2_DISPATCH(stream_attempt_was_cancelled);
    CCH2F2_DISPATCH(mark_finish_seen);
    CCH2F2_DISPATCH(quiet);
#undef CCH2F2_DISPATCH
    return cch2_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only; fixtures may carry raw U+2028 inside strings) ---- */
static char *cch2_h_readline(FILE *f)
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

static void cch2_h_run(const char *func, json_t *in)
{
    cch2_script_t sc;
    sc.io = cch2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cch2_io_t io;
    io.call = cch2_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = cch2_h_entry(func, &io, in, st);

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
    while ((line = cch2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cch2_str(cch2_get(rec, "func"));
        json_t *in = cch2_get(rec, "in");
        if (func && in) cch2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
