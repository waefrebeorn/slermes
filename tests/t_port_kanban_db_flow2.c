/* t_port_kanban_db_flow2.c — harness for the hermes_cli/kanban_db.py
 * UNCLAIMED-remainder FLOW port #2 (src/cli/kanban_db_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <live_name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-event sequence ({"call":name,...args}); "state"
 * is {} (no ported flow mutates module state; the pin ContextVar is seamed).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret"/"raise"} or a per-call
 * queue LIST). The dispatcher applies it exactly as the oracle's recording
 * stubs are fed. Missing/exhausted scripts FAIL CLOSED with a RuntimeError
 * "no script for <name>" raise. "import:" seams model in-function imports:
 * success records nothing (__untrace__), failure raises (__untrace__ +
 * __raise__). The harness echoes "in" with json_copy (json_set takes
 * ownership); borrowed inputs are never mutated by the port.
 */
#include "hermes_json.h"
#include "cli/kanban_db_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } kd2_script_ctx_t;

static json_t *kd2_script_next(kd2_script_ctx_t *sc, const char *name) {
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

static json_t *kd2_dispatch(kd2_io_t *io, const char *name, json_t *args) {
    (void)args;
    kd2_script_ctx_t *sc = (kd2_script_ctx_t *)io->ctx;
    if (strncmp(name, "import:", 7) == 0) {
        json_t *entry = sc && sc->io ? json_obj_get(sc->io, name) : NULL;
        json_t *out = json_object();
        json_set(out, "__untrace__", json_bool(true));
        if (entry && entry->type == JSON_OBJECT && json_obj_get(entry, "import_raise")) {
            json_t *spec = json_obj_get(entry, "import_raise");
            json_set(out, "__raise__", json_copy(spec));
        }
        return out;
    }
    json_t *result = kd2_script_next(sc, name);
    if (!result) {
        char *msg = kd2_asprintf("no script for %s", name);
        json_t *e = kd2_raise("RuntimeError", msg);
        free(msg);
        return e;
    }
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) return kd2_raise_from_spec(raise);
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static json_t *kd2_run_case(const char *func, json_t *in) {
    kd2_io_t io;
    kd2_script_ctx_t sc;
    io.call = kd2_dispatch;
    io.trace = json_array();
    io.state = json_object();
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    io.ctx = &sc;
    json_t *ret = NULL;

    if (strcmp(func, "pin_first_board_resolution") == 0) {
        ret = kd2_pin_first_board_resolution(&io, json_obj_get(in, "body"));
    } else if (strcmp(func, "_normalize_board_slug") == 0) {
        ret = kd2_normalize_board_slug(&io, json_obj_get(in, "slug"));
    } else if (strcmp(func, "_resolve_project_link") == 0) {
        ret = kd2_resolve_project_link(&io, json_obj_get(in, "project_id"),
                                       json_obj_get(in, "project_source_task_id"),
                                       json_get_str(in, "workspace_kind", "scratch"),
                                       json_obj_get(in, "workspace_path"));
    } else if (strcmp(func, "_project_from_source_task") == 0) {
        ret = kd2_project_from_source_task(&io, json_get_str(in, "project_id", ""),
                                           json_get_str(in, "source_task_id", ""));
    } else if (strcmp(func, "_stage_completion_artifacts") == 0) {
        ret = kd2_stage_completion_artifacts(&io, json_get_str(in, "task_id", ""),
                                             json_obj_get(in, "metadata"),
                                             (long long)json_get_num(in, "now", 0),
                                             json_get_str(in, "uploaded_by", "kanban_complete"));
    } else if (strcmp(func, "_discard_staged_copies") == 0) {
        ret = kd2_discard_staged_copies(&io, json_obj_get(in, "copies"),
                                        json_get_str(in, "attachment_dir", ""));
    } else if (strcmp(func, "_copy_capped") == 0) {
        ret = kd2_copy_capped(&io, json_get_str(in, "src", ""), json_get_str(in, "dest", ""),
                              json_get_str(in, "artifact", ""),
                              (long long)json_get_num(in, "cap", (double)KD2_ATTACHMENT_MAX_BYTES));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", json_object());
    json_free(io.trace);
    json_free(io.state);
    json_free(sc.cursors);
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
        json_t *out = kd2_run_case(func, in);
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