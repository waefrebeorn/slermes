/* t_port_code_execution_flow.c — harness for the execute_code FLOW + leaf port
 * (src/tools/code_execution_flow.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret"), and the echoed mutable state ("state"). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim), a LIST consumed in call order (a
 * queue), or {"__script__": true, "ret": <value>, "raise": {type,message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained; the gate links only
 * lib/libjson/json.o).
 */
#include "hermes_json.h"
#include "tools/code_execution_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io_map;
    json_t *cursors;
} ce_script_ctx_t;

static json_t *ce_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *ce_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ce_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *ce_dispatch_script(ce_io_t *io, const char *name, json_t *args) {
    (void)args;
    ce_script_ctx_t *sc = (ce_script_ctx_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", "RuntimeError");
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set_hdr(e, "message", msg);
        return e;
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            json_t *e = json_object();
            json_set_hdr(e, "__raise__", "RuntimeError");
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            json_set_hdr(e, "message", msg);
            return e;
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return ce_entry_result(json_get(entry, idx));
    }
    return ce_entry_result(entry);
}

static char *ce_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = ce_read_line(f)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();

        ce_io_t io;
        ce_script_ctx_t sc;
        io.call = ce_dispatch_script;
        io.trace = json_array();
        json_t *st = json_obj_get(in, "state");
        io.state = (st && st->type == JSON_OBJECT) ? json_copy(st) : json_object();
        io.ctx = &sc;
        sc.io_map = json_obj_get(in, "io");
        sc.cursors = json_object();

        json_t *r = ce_dispatch(&io, func, in);
        json_t *out = json_object();
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "trace", json_copy(io.trace));
        json_set(out, "state", json_copy(io.state));

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(io.trace);
        json_free(io.state);
        json_free(sc.cursors);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}