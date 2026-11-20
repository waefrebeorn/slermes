/* t_port_chat_completion_helpers_flow3.c -- harness for the FLOW3 remainder of
 * agent/chat_completion_helpers.py (scoped to 15 verified-clean defs, 75 cases).
 *
 * Reads one JSON object per line from stdin (or argv[1]):
 *   {"func": <live_name>[@<lineno>], "in": {"objs":{...},"args":[...],
 *      "kwargs":{...},"globals":{...},"io":{...}}}
 * Emits one JSON object per line: {"func","in","ret","trace","st"}.
 * Dumb emitter: no Python, no expected values. The oracle diffs against the
 * live CPython drive of the same defs with the same seam scripts.
 */
#include "agent/chat_completion_helpers_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io;
    json_t *cursors;
    json_t *trace;
} cch3_script_t;

static json_t *cch3_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy((json_t *)v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cch3_h_strip_nulls(val));
    }
    return o;
}

static json_t *cch3_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cch3_script_t *sc = (cch3_script_t *)ctx;
    json_t *entry = sc->io ? json_obj_get(sc->io, (char *)name) : NULL;
    int present = entry != NULL, exhausted = 0;
    json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, (char *)name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else { result = json_get(entry, idx); json_set(sc->cursors, (char *)name, json_number(cur + 1)); }
        } else result = entry;
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
        char *m = cch3_xfmt("no script for %s", name);
        retnode = cch3_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else retnode = actual ? json_copy(actual) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);
    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

static char *cch3_h_readline(FILE *f)
{
    size_t cap = 8192, n = 0;
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

static void cch3_h_run(const char *func, json_t *in)
{
    cch3_script_t sc;
    sc.io = cch3_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cch3_io_t io;
    io.call = cch3_h_dispatch;
    io.ctx = &sc;
    json_t *objs = (json_t *)cch3_get(in, "objs");
    json_t *st = objs ? json_copy(objs) : json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = cch3_entry(func, &io, in, st);
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
    while ((line = cch3_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cch3_str(cch3_get(rec, "func"));
        json_t *in = cch3_get(rec, "in");
        if (func && in) cch3_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
