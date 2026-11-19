/* t_port_status_phrases_flow.c — harness for the status-phrases FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret"), and — for the state-mutating flows — the mutated state ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives live from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim), a LIST consumed in call order (a queue),
 * or {"__script__": true, "ret": <value>, "raise": {type,message}}. A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * Self-contained: includes the port .c by area-qualified path; libjson only.
 */
#include "hermes_json.h"
#include "gateway/status_phrases_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io_map;
    json_t *cursors;
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(sph_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
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
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*sph_fn_t)(sph_io_t *, json_t *);
/* state_kind: 0 = none, 1 = catalog, 2 = recent */
typedef struct { const char *func; sph_fn_t fn; int state_kind; } sph_case_t;

static const sph_case_t CASES[] = {
    { "_clean_phrase_list", sph_clean_phrase_list, 0 },
    { "_copy_catalog", sph_copy_catalog, 0 },
    { "_merge_phrase_mapping", sph_merge_phrase_mapping, 1 },
    { "_merge_phrase_file", sph_merge_phrase_file, 1 },
    { "_iter_phrase_files", sph_iter_phrase_files, 0 },
    { "_merge_phrase_paths", sph_merge_phrase_paths, 1 },
    { "_merge_phrase_config", sph_merge_phrase_config, 1 },
    { "resolve_status_phrase_catalog", sph_resolve_status_phrase_catalog, 0 },
    { "classify_status_context", sph_classify_status_context, 0 },
    { "choose_status_phrase", sph_choose_status_phrase, 2 },
    { "_load_default_phrases", sph_load_default_phrases, 0 },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
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

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const sph_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            sph_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            if (tc->state_kind == 1) {
                json_t *state = json_object();
                json_t *cat = json_obj_get(pin, "catalog");
                json_set(state, "catalog", cat ? json_copy(cat) : json_null());
                json_set(out, "state", state);
            } else if (tc->state_kind == 2) {
                json_t *state = json_object();
                json_t *rec_node = json_obj_get(pin, "recent");
                json_set(state, "recent", rec_node ? json_copy(rec_node) : json_null());
                json_set(out, "state", state);
            }
            json_free(pin);
            json_free(io.trace);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}