/* t_port_tui_dispatch_flow.c — harness for the hermes_cli/cli_tui_mixin.py
 * TUI-dispatch FLOW port (src/cli/port_tui_dispatch_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace", "state"}}.
 * "trace" is the ordered seam-event sequence ({ev, args, ret}); "state" the
 * mutated CLI-instance / module-global mirror (full schema, seeded by the
 * case). Scripted I/O lives in in.io (seam-name -> {"ret", "state", "raise",
 * "import_raise"} or a queue LIST consumed per call), applied by the
 * dispatcher exactly as the oracle's recording stubs are fed. An
 * "import_raise" entry on an "import:<module>" key models a failed
 * in-function import: the dispatcher returns an untrace marker so the live
 * import statement records nothing.
 *
 * Built-in doubles (buffer.reset/delete, app.invalidate/exit, queue puts,
 * gateway_command_wait.set) are implemented inside the port and record
 * their own events; they are never scripted.
 */
#include "hermes_json.h"
#include "port_tui_dispatch_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} ctd_script_ctx_t;

static json_t *ctd_script_next(ctd_script_ctx_t *sc, const char *name) {
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

static void ctd_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *ctd_dispatch(ctd_io_t *io, const char *name, json_t *args) {
    (void)args;
    ctd_script_ctx_t *sc = (ctd_script_ctx_t *)io->ctx;
    json_t *entry = ctd_script_next(sc, name);
    if (!entry) return json_null();
    if (entry->type == JSON_OBJECT) {
        ctd_apply_state_effects(io->state, entry);
        json_t *imp = json_obj_get(entry, "import_raise");
        if (imp && imp->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(imp));
            json_set(out, "__untrace__", json_bool(true));
            return out;
        }
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(raise));
            return out;
        }
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(entry);
}

static json_t *ctd_run_case(const char *func, json_t *in) {
    ctd_io_t io;
    ctd_script_ctx_t sc;
    json_t *state = json_obj_get(in, "state");
    state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
    io.call = ctd_dispatch;
    io.trace = json_array();
    io.state = state;
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    json_t *ret = NULL;

    if (strcmp(func, "handle_enter") == 0) {
        ret = ctd_handle_enter_flow(&io);
    } else if (strcmp(func, "submit_buffer") == 0) {
        ret = ctd_submit_buffer_flow(&io);
    } else if (strcmp(func, "enter_inline_command") == 0) {
        ret = ctd_enter_inline_command_flow(&io, json_get_str(in, "text", ""),
                                            json_get_bool(in, "has_images", false));
    } else if (strcmp(func, "enter_while_busy") == 0) {
        ret = ctd_enter_while_busy_flow(&io, json_get_str(in, "text", ""),
                                        json_obj_get(in, "images"),
                                        json_obj_get(in, "payload"),
                                        json_get_str(in, "mode", NULL));
    } else if (strcmp(func, "enter_overlay") == 0) {
        ret = ctd_enter_overlay_flow(&io);
    } else if (strcmp(func, "enter_clarify_freetext") == 0) {
        ret = ctd_enter_clarify_freetext_flow(&io);
    } else if (strcmp(func, "enter_clarify_choice") == 0) {
        ret = ctd_enter_clarify_choice_flow(&io);
    } else if (strcmp(func, "handle_ctrl_c") == 0) {
        ret = ctd_handle_ctrl_c_flow(&io);
    } else if (strcmp(func, "handle_ctrl_q") == 0) {
        ret = ctd_handle_ctrl_q_flow(&io);
    } else if (strcmp(func, "handle_escape_modal") == 0) {
        ret = ctd_handle_escape_modal_flow(&io);
    } else if (strcmp(func, "handle_ctrl_d") == 0) {
        ret = ctd_handle_ctrl_d_flow(&io);
    } else if (strcmp(func, "clear_or_exit") == 0) {
        ret = ctd_clear_or_exit_flow(&io);
    } else if (strcmp(func, "clear_blocking_overlays") == 0) {
        ret = ctd_clear_blocking_overlays_flow(&io);
    } else if (strcmp(func, "cancel_foreground_ui") == 0) {
        ret = ctd_cancel_foreground_ui_flow(&io, json_obj_get(in, "closers"));
    } else if (strcmp(func, "connection_move") == 0) {
        ret = ctd_connection_move_flow(&io, (int)json_get_num(in, "delta", 0));
    } else if (strcmp(func, "model_picker_escape") == 0) {
        ret = ctd_model_picker_escape_flow(&io);
    } else if (strcmp(func, "paste_over_threshold") == 0) {
        ret = ctd_paste_over_threshold(&io, json_get_str(in, "text", ""),
                                       (long long)json_get_num(in, "line_count", 0),
                                       json_get_str(in, "threshold_key", "paste_collapse_threshold"));
    } else if (strcmp(func, "suppress_closed_loop_errors") == 0) {
        ret = ctd_suppress_closed_loop_errors_flow(&io, json_obj_get(in, "context"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", json_copy(state));
    json_free(io.trace);
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
        json_t *out = ctd_run_case(func, in);
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
