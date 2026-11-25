/* t_port_kanban_adapter_flow.c — harness for the hermes_cli/kanban_db.py
 * UNCLAIMED-remainder FLOW port (src/cli/kanban_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <live_name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-event sequence ({"call":name,...args}); "state"
 * is {} (no ported flow mutates module state).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret"/"raise"} or a per-call
 * queue LIST). The dispatcher applies it exactly as the oracle's recording
 * stubs are fed. Missing/exhausted scripts FAIL CLOSED with a RuntimeError
 * "no script for <name>" raise. "import:" seams model in-function imports:
 * success records nothing (__untrace__), failure raises (__untrace__ +
 * __raise__). The harness echoes "in" with json_copy (json_set takes
 * ownership).
 */
#include "hermes_json.h"
#include "cli/kanban_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } ka_script_ctx_t;

static json_t *ka_script_next(ka_script_ctx_t *sc, const char *name) {
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

static json_t *ka_dispatch(ka_io_t *io, const char *name, json_t *args) {
    (void)args;
    ka_script_ctx_t *sc = (ka_script_ctx_t *)io->ctx;
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
    json_t *result = ka_script_next(sc, name);
    if (!result) {
        char *msg = ka_asprintf("no script for %s", name);
        json_t *e = ka_raise("RuntimeError", msg);
        free(msg);
        return e;
    }
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) return ka_raise_from_spec(raise);
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static json_t *ka_run_case(const char *func, json_t *in) {
    ka_io_t io;
    ka_script_ctx_t sc;
    io.call = ka_dispatch;
    io.trace = json_array();
    io.state = json_object();
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    io.ctx = &sc;
    json_t *ret = NULL;

    if (strcmp(func, "_lossy_text") == 0) {
        ret = ka_lossy_text(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_row_get") == 0) {
        ret = ka_row_get(&io, json_obj_get(in, "row"), json_get_str(in, "col", ""),
                         json_obj_get(in, "default"));
    } else if (strcmp(func, "_json_or") == 0) {
        ret = ka_json_or(&io, json_obj_get(in, "value"), json_obj_get(in, "default"));
    } else if (strcmp(func, "_json_dict") == 0) {
        ret = ka_json_dict(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_env_int") == 0) {
        ret = ka_env_int(&io, json_get_str(in, "name", ""), json_obj_get(in, "default"),
                         (long long)json_get_num(in, "minimum", 0));
    } else if (strcmp(func, "_first_line") == 0) {
        ret = ka_first_line(&io, json_obj_get(in, "text"),
                            (long long)json_get_num(in, "limit", 0));
    } else if (strcmp(func, "_opt_int") == 0) {
        ret = ka_opt_int(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_json_or_null") == 0) {
        ret = ka_json_or_null(&io, json_obj_get(in, "obj"));
    } else if (strcmp(func, "_substantive_text") == 0) {
        ret = ka_substantive_text(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_nonblank_str") == 0) {
        ret = ka_nonblank_str(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_cleaned_artifact_paths") == 0) {
        ret = ka_cleaned_artifact_paths(&io, json_obj_get(in, "metadata"));
    } else if (strcmp(func, "_retention_seconds") == 0) {
        ret = ka_retention_seconds(&io, json_obj_get(in, "older_than_seconds"));
    } else if (strcmp(func, "_slug_or_default") == 0) {
        ret = ka_slug_or_default(&io, json_obj_get(in, "board"));
    } else if (strcmp(func, "_require_slug") == 0) {
        ret = ka_require_slug(&io, json_obj_get(in, "slug"));
    } else if (strcmp(func, "_host_prefix") == 0) {
        ret = ka_host_prefix(&io);
    } else if (strcmp(func, "_board_meta_for") == 0) {
        ret = ka_board_meta_for(&io, json_obj_get(in, "board"));
    } else if (strcmp(func, "_hook_profile_name") == 0) {
        ret = ka_hook_profile_name(&io);
    } else if (strcmp(func, "_fire_task_hook") == 0) {
        ret = ka_fire_task_hook(&io, json_get_str(in, "event", ""), json_obj_get(in, "task"),
                                json_get_str(in, "task_id", ""), json_obj_get(in, "run_id"),
                                json_obj_get(in, "fields"));
    } else if (strcmp(func, "_explicit_board_slug") == 0) {
        ret = ka_explicit_board_slug(&io, json_obj_get(in, "board"));
    } else if (strcmp(func, "_explicit_board_intent_pinned") == 0) {
        ret = ka_explicit_board_intent_pinned(&io);
    } else if (strcmp(func, "_board_path") == 0) {
        ret = ka_board_path(&io, json_obj_get(in, "env_var"), json_obj_get(in, "board"),
                            json_obj_get(in, "default_parts"), json_get_str(in, "leaf", ""));
    } else if (strcmp(func, "_link") == 0) {
        ret = ka_link(&io, json_get_str(in, "parent_id", ""), json_get_str(in, "child_id", ""));
    } else if (strcmp(func, "_linked_ids") == 0) {
        ret = ka_linked_ids(&io, json_get_str(in, "want", ""), json_get_str(in, "where", ""),
                            json_get_str(in, "task_id", ""));
    } else if (strcmp(func, "_require_task") == 0) {
        ret = ka_require_task(&io, json_get_str(in, "task_id", ""));
    } else if (strcmp(func, "_task_rows") == 0) {
        ret = ka_task_rows(&io, json_get_str(in, "table", ""), json_get_str(in, "task_id", ""),
                           json_get_str(in, "order", ""));
    } else if (strcmp(func, "_latest_event") == 0) {
        ret = ka_latest_event(&io, json_get_str(in, "task_id", ""), json_get_str(in, "kind", ""),
                              json_obj_get(in, "run_id"));
    } else if (strcmp(func, "_counts_by_assignee") == 0) {
        ret = ka_counts_by_assignee(&io);
    } else if (strcmp(func, "current_run_started_ats") == 0) {
        ret = ka_current_run_started_ats(&io, json_obj_get(in, "task_ids"));
    } else if (strcmp(func, "_end_or_synthesize_run") == 0) {
        ret = ka_end_or_synthesize_run(&io, json_get_str(in, "task_id", ""),
                                       json_get_str(in, "outcome", ""),
                                       json_get_str(in, "status", ""),
                                       json_obj_get(in, "summary"), json_obj_get(in, "metadata"),
                                       json_get_bool(in, "synthesize", false),
                                       json_obj_get(in, "profile"), json_has(in, "profile"));
    } else if (strcmp(func, "_gate_created_cards") == 0) {
        ret = ka_gate_created_cards(&io, json_get_str(in, "task_id", ""),
                                    json_obj_get(in, "created_cards"),
                                    json_obj_get(in, "preview_text"));
    } else if (strcmp(func, "_flag_phantom_prose_refs") == 0) {
        ret = ka_flag_phantom_prose_refs(&io, json_get_str(in, "task_id", ""),
                                         json_obj_get(in, "run_id"),
                                         json_obj_get(in, "summary"), json_obj_get(in, "result"),
                                         json_obj_get(in, "verified_cards"));
    } else if (strcmp(func, "_git_out") == 0) {
        ret = ka_git_out(&io, json_get_str(in, "cwd", ""), json_obj_get(in, "args"),
                         (long long)json_get_num(in, "timeout", 30));
    } else if (strcmp(func, "_project_branch_name") == 0) {
        ret = ka_project_branch_name(&io, json_obj_get(in, "project_obj"),
                                     json_get_str(in, "task_id", ""), json_obj_get(in, "title"));
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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = ka_run_case(func, in);
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