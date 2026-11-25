/* t_port_projects_db_flow.c — harness for the hermes_cli/projects_db.py
 * worst-tier FLOW port (src/cli/projects_db_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <live_name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-event sequence ({"call":name,...args});
 * "state" is {"initialized_paths": sorted} — the _INITIALIZED_PATHS module
 * set, seeded from in.state.initialized_paths (absent -> []) and sorted on
 * echo (the live set iterates arbitrarily; the oracle echoes sorted too).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret"/"raise"} or a per-call
 * queue LIST). The dispatcher applies it exactly as the oracle's recording
 * stubs are fed. Missing/exhausted scripts FAIL CLOSED with a RuntimeError
 * "no script for <name>" raise. The harness echoes "in" with json_copy
 * (json_set takes ownership); borrowed inputs are never mutated by the port.
 */
#include "hermes_json.h"
#include "cli/projects_db_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } pdb_script_ctx_t;

static json_t *pdb_script_next(pdb_script_ctx_t *sc, const char *name) {
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

static json_t *pdb_h_dispatch(pdb_io_t *io, const char *name, json_t *args) {
    (void)args;
    pdb_script_ctx_t *sc = (pdb_script_ctx_t *)io->ctx;
    json_t *result = pdb_script_next(sc, name);
    if (!result) {
        char *msg = pdb_asprintf("no script for %s", name);
        json_t *e = pdb_raise("RuntimeError", msg);
        free(msg);
        return e;
    }
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) return pdb_raise_from_spec(raise);
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static int pdb_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* out.state: {"initialized_paths": sorted} (set semantics on both sides). */
static json_t *pdb_sorted_state(json_t *state) {
    json_t *out = json_object();
    json_t *sorted = json_array();
    json_t *arr = state ? json_obj_get(state, "initialized_paths") : NULL;
    if (arr && arr->type == JSON_ARRAY && arr->c.count > 0) {
        size_t n = arr->c.count;
        const char **strs = malloc(n * sizeof(char *));
        if (strs) {
            for (size_t i = 0; i < n; i++) {
                const char *s = pdb_str(arr->c.items[i]);
                strs[i] = s ? s : "";
            }
            qsort(strs, n, sizeof(char *), pdb_str_cmp);
            for (size_t i = 0; i < n; i++)
                json_append(sorted, json_string(strs[i]));
            free(strs);
        }
    }
    json_set(out, "initialized_paths", sorted);
    return out;
}

static json_t *pdb_run_case(const char *func, json_t *in) {
    pdb_io_t io;
    pdb_script_ctx_t sc;
    io.call = pdb_h_dispatch;
    io.trace = json_array();
    io.state = json_object();
    json_t *st = json_obj_get(in, "state");
    json_t *paths = st ? json_obj_get(st, "initialized_paths") : NULL;
    json_set(io.state, "initialized_paths",
             (paths && paths->type == JSON_ARRAY) ? json_copy(paths) : json_array());
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    io.ctx = &sc;

    json_t *ret = pdb_dispatch(func, in, &io);

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", pdb_sorted_state(io.state));
    json_free(io.trace);
    json_free(io.state);
    json_free(sc.cursors);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *in_owned = NULL;
        if (!in) { in_owned = json_object(); in = in_owned; }
        json_t *out = pdb_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(in_owned);
        json_free(rec);
    }
    return 0;
}