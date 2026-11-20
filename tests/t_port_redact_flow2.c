/* t_port_redact_flow2.c — harness for the agent/redact.py FLOW2 oracle.
 *
 * Reads JSONL: {"func": <name>, "in": {...}}.
 * Emits JSONL: {"func", "in", "out": {"ret", "trace", "state"}}.
 * Scripted I/O lives in in.io (seam-name -> {"ret":v} | {"raise":{...}} | [queue]).
 */
#include "hermes_json.h"
#include "agent/redact_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} rdf2_script_ctx_t;

static json_t *rdf2_script_next(rdf2_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}

static json_t *rdf2_dispatch_seam(rdf2_io_t *io, const char *name, json_t *args) {
    (void)args;
    rdf2_script_ctx_t *sc = (rdf2_script_ctx_t *)io->ctx;
    json_t *result = rdf2_script_next(sc, name);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_t *t = json_obj_get(raise, "type");
            json_set(exc, "__raise__", t ? json_copy(t) : json_string("Exception"));
            json_t *msg = json_obj_get(raise, "message");
            json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return result ? json_copy(result) : json_null();
}

static void rdf2_io_init(rdf2_io_t *io, rdf2_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = rdf2_dispatch_seam;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *rdf2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *rdf2_make_out(rdf2_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *rdf2_run_case(const char *func, json_t *in) {
    rdf2_io_t io;
    rdf2_script_ctx_t sc;
    json_t *state = rdf2_state_from(in);
    rdf2_io_init(&io, &sc, in, state);
    json_t *ret = rdf2_dispatch(&io, func, in);
    json_t *out = rdf2_make_out(&io, ret);
    json_free(sc.cursors);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = rdf2_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}
