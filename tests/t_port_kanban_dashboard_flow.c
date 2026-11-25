/* t_port_kanban_dashboard_flow.c — harness for the plugins/kanban/dashboard/
 * plugin_api.py FLOW port (src/hermes_cli/kanban_dashboard_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <live_name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-event sequence ({"call":name,...args}); "state"
 * carries the mutated task_d for _attach_diagnostics (else {}).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret"/"raise"/"import_raise"} or a
 * per-call queue LIST). The dispatcher applies it exactly as the oracle's
 * recording stubs are fed. Missing/exhausted scripts FAIL CLOSED with a
 * RuntimeError "no script for <name>" raise. "import:" seams model in-function
 * imports: success records nothing (__untrace__), failure raises (__untrace__ +
 * __raise__). The harness echoes "in" with json_copy (json_set takes ownership).
 */
#include "hermes_json.h"
#include "hermes_cli/kanban_dashboard_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } kd_script_ctx_t;

static json_t *kd_script_next(kd_script_ctx_t *sc, const char *name) {
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

static json_t *kd_dispatch(kd_io_t *io, const char *name, json_t *args) {
    (void)args;
    kd_script_ctx_t *sc = (kd_script_ctx_t *)io->ctx;
    if (strncmp(name, "import:", 7) == 0) {
        json_t *entry = json_obj_get(sc->io, name);
        json_t *out = json_object();
        json_set(out, "__untrace__", json_bool(true));
        if (entry && entry->type == JSON_OBJECT && json_obj_get(entry, "import_raise")) {
            json_t *result = kd_script_next(sc, name);
            json_t *spec = result ? json_obj_get(result, "import_raise") : NULL;
            if (spec) json_set(out, "__raise__", json_copy(spec));
        }
        return out;
    }
    json_t *result = kd_script_next(sc, name);
    if (!result) {
        char *msg = kd_asprintf("no script for %s", name);
        json_t *e = kd_make_raise("RuntimeError", msg);
        free(msg);
        return e;
    }
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) return kd_raise_from_spec(raise);
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static json_t *kd_run_case(const char *func, json_t *in) {
    kd_io_t io;
    kd_script_ctx_t sc;
    io.call = kd_dispatch;
    io.trace = json_array();
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    io.ctx = &sc;
    json_t *ret = NULL;
    json_t *state = json_object();

    if (strcmp(func, "_placeholders") == 0) {
        ret = kd_placeholders(&io, json_obj_get(in, "ids"));
    } else if (strcmp(func, "_cap") == 0) {
        ret = kd_cap(&io, json_obj_get(in, "s"), json_get_num(in, "n", 0));
    } else if (strcmp(func, "_attachment_dict") == 0) {
        ret = kd_attachment_dict(&io, json_obj_get(in, "a"));
    } else if (strcmp(func, "_board_display_kwargs") == 0) {
        ret = kd_board_display_kwargs(&io, json_obj_get(in, "p"));
    } else if (strcmp(func, "_warnings_summary_from_diagnostics") == 0) {
        ret = kd_warnings_summary(&io, json_obj_get(in, "diagnostics"));
    } else if (strcmp(func, "_require_ok") == 0) {
        ret = kd_require_ok(&io, json_obj_get(in, "ok"));
    } else if (strcmp(func, "_conflict") == 0) {
        ret = kd_conflict(&io, json_get_str(in, "detail", ""));
    } else if (strcmp(func, "_require") == 0) {
        ret = kd_require(&io, json_get_str(in, "getter_name", "kanban_db.get_task"),
                         json_get_str(in, "ident_key", "task_id"),
                         json_obj_get(in, "ident"), json_get_str(in, "label", "task"));
    } else if (strcmp(func, "_require_task") == 0) {
        ret = kd_require_task(&io, json_obj_get(in, "task_id"));
    } else if (strcmp(func, "_require_run") == 0) {
        ret = kd_require_run(&io, json_obj_get(in, "run_id"));
    } else if (strcmp(func, "_normalize_slug_or_400") == 0) {
        ret = kd_normalize_slug_or_400(&io, json_obj_get(in, "slug"));
    } else if (strcmp(func, "_existing_board_slug") == 0) {
        ret = kd_existing_board_slug(&io, json_obj_get(in, "slug"));
    } else if (strcmp(func, "_ws_upgrade_authorized") == 0) {
        ret = kd_ws_upgrade_authorized(&io);
    } else if (strcmp(func, "_since_param") == 0) {
        ret = kd_since_param(&io, json_obj_get(in, "query_params"));
    } else if (strcmp(func, "_ws_board") == 0) {
        ret = kd_ws_board(&io, json_obj_get(in, "raw"));
    } else if (strcmp(func, "_ws_board_live") == 0) {
        ret = kd_ws_board_live(&io, json_obj_get(in, "normed"));
    } else if (strcmp(func, "_load_config_or_empty") == 0) {
        ret = kd_load_config_or_empty(&io);
    } else if (strcmp(func, "_active_profile_name") == 0) {
        ret = kd_active_profile_name(&io);
    } else if (strcmp(func, "_resolve_board") == 0) {
        ret = kd_resolve_board(&io, json_obj_get(in, "board"));
    } else if (strcmp(func, "_conn") == 0) {
        ret = kd_conn(&io, json_obj_get(in, "board"));
    } else if (strcmp(func, "_attach_diagnostics") == 0) {
        json_t *td_in = json_obj_get(in, "task_d");
        json_t *td = td_in ? json_copy(td_in) : json_object();
        ret = kd_attach_diagnostics(&io, td, json_obj_get(in, "diags"));
        json_set(state, "task_d", td);
    } else if (strcmp(func, "_task_dict") == 0) {
        ret = kd_task_dict(&io, json_obj_get(in, "task"), json_obj_get(in, "latest_summary"),
                           json_obj_get(in, "current_run_started_at"));
    } else if (strcmp(func, "_links_for") == 0) {
        ret = kd_links_for(&io, json_obj_get(in, "task_id"));
    } else if (strcmp(func, "_link_tasks") == 0) {
        ret = kd_link_tasks(&io, json_obj_get(in, "links"));
    } else if (strcmp(func, "_drag_to") == 0) {
        ret = kd_drag_to(&io, json_obj_get(in, "task_id"), json_get_str(in, "s", ""));
    } else if (strcmp(func, "_apply_status") == 0) {
        ret = kd_apply_status(&io, json_obj_get(in, "task_id"), json_get_str(in, "s", ""),
                              json_obj_get(in, "p"), json_get_str(in, "unknown_detail", ""));
    } else if (strcmp(func, "_set_priority") == 0) {
        ret = kd_set_priority(&io, json_obj_get(in, "task_id"), json_obj_get(in, "priority"),
                              json_obj_get(in, "board"));
    } else if (strcmp(func, "_apply_model_override") == 0) {
        ret = kd_apply_model_override(&io, json_obj_get(in, "task_id"), json_obj_get(in, "p"));
    } else if (strcmp(func, "_apply_reasoning_effort") == 0) {
        ret = kd_apply_reasoning_effort(&io, json_obj_get(in, "task_id"), json_obj_get(in, "p"));
    } else if (strcmp(func, "_open_parent_refusal") == 0) {
        ret = kd_open_parent_refusal(&io, json_obj_get(in, "task_id"), json_get_str(in, "s", ""));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", state);
    json_free(io.trace);
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
        json_t *out = kd_run_case(func, in);
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
