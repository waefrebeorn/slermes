/* t_port_cli_tui_mixin_flow.c — harness for the hermes_cli/cli_tui_mixin.py
 * navigation/editor/stash FLOW port (src/cli/cli_tui_mixin_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace", "state"}}.
 * "trace" is the ordered seam-event sequence ({ev, args, ret}); "state" the
 * mutated CLI-instance mirror (seeded by the case). Scripted I/O lives in
 * in.io (seam-name -> {"ret", "state", "raise", "import_raise"} or a queue
 * LIST consumed per call), applied by the dispatcher exactly as the oracle's
 * recording stubs are fed. An "import_raise" entry on an "import:<module>"
 * key models a failed in-function import (untrace marker, no trace token).
 *
 * Built-in doubles (buffer.reset/insert_text, app.invalidate) are implemented
 * inside the port and record their own events; they are never scripted.
 * buffer.auto_up/auto_down ARE scripted seams whose "buf" state effect is
 * merged in place so the composer mirror stays attached.
 */
#include "hermes_json.h"
#include "cli/cli_tui_mixin_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} ctm_script_ctx_t;

static json_t *ctm_script_next(ctm_script_ctx_t *sc, const char *name) {
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

static void ctm_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++) {
        const char *k = eff->c.keys[i];
        json_t *v = eff->c.items[i];
        /* merge "buf" in place so the BufDouble mirror stays attached */
        if (strcmp(k, "buf") == 0 && v && v->type == JSON_OBJECT) {
            json_t *b = json_obj_get(state, "buf");
            if (b && b->type == JSON_OBJECT) {
                for (size_t j = 0; j < v->c.count; j++)
                    json_set(b, v->c.keys[j], json_copy(v->c.items[j]));
                continue;
            }
        }
        json_set(state, k, json_copy(v));
    }
}

static json_t *ctm_dispatch(ctm_io_t *io, const char *name, json_t *args) {
    (void)args;
    ctm_script_ctx_t *sc = (ctm_script_ctx_t *)io->ctx;
    json_t *entry = ctm_script_next(sc, name);
    if (!entry) return json_null();
    if (entry->type == JSON_OBJECT) {
        ctm_apply_state_effects(io->state, entry);
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

static json_t *ctm_run_case(const char *func, json_t *in) {
    ctm_io_t io;
    ctm_script_ctx_t sc;
    json_t *state = json_obj_get(in, "state");
    state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
    io.call = ctm_dispatch;
    io.trace = json_array();
    io.state = state;
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    json_t *ret = NULL;
    json_t *state_out = NULL;   /* set_filter emits its st object as state */

#define ARGS0 json_object()
    if (strcmp(func, "approval_choice_label") == 0) {
        ret = ctm_approval_choice_label(&io, json_get_str(in, "choice", ""));
    } else if (strcmp(func, "agent_name") == 0) {
        ret = ctm_agent_name(&io);
    } else if (strcmp(func, "tn") == 0) {
        json_t *kw = json_obj_get(in, "kwargs");
        ret = ctm_tn(&io, json_get_str(in, "key", ""),
                     (long long)json_get_num(in, "count", 0),
                     kw ? json_copy(kw) : json_object());
    } else if (strcmp(func, "num_prefix") == 0) {
        ret = ctm_num_prefix((long long)json_get_num(in, "i", 0));
    } else if (strcmp(func, "term_rows") == 0) {
        ret = ctm_term_rows(&io);
    } else if (strcmp(func, "set_filter") == 0) {
        json_t *st = json_obj_get(in, "st");
        st = (st && st->type == JSON_OBJECT) ? json_copy(st) : json_object();
        ret = ctm_set_filter_flow(st, json_get_str(in, "value", ""));
        state_out = st;
    } else if (strcmp(func, "audio_level_bar") == 0) {
        ret = ctm_audio_level_bar(&io);
    } else if (strcmp(func, "connection_up") == 0) {
        ret = ctm_connection_up(&io);
    } else if (strcmp(func, "connection_down") == 0) {
        ret = ctm_connection_down(&io);
    } else if (strcmp(func, "connection_side") == 0) {
        ret = ctm_connection_side(&io);
    } else if (strcmp(func, "model_picker_down") == 0) {
        ret = ctm_model_picker_down(&io);
    } else if (strcmp(func, "model_picker_up") == 0) {
        ret = ctm_model_picker_up(&io);
    } else if (strcmp(func, "model_picker_filter_backspace") == 0) {
        ret = ctm_model_picker_filter_backspace(&io);
    } else if (strcmp(func, "clarify_toggle") == 0) {
        ret = ctm_clarify_toggle(&io);
    } else if (strcmp(func, "clarify_down") == 0) {
        ret = ctm_clarify_down(&io);
    } else if (strcmp(func, "clarify_up") == 0) {
        ret = ctm_clarify_up(&io);
    } else if (strcmp(func, "clarify_batch_step") == 0) {
        ret = ctm_clarify_batch_step(&io, (long long)json_get_num(in, "delta", 0));
    } else if (strcmp(func, "clarify_batch_tab") == 0) {
        ret = ctm_clarify_batch_tab(&io);
    } else if (strcmp(func, "clarify_batch_backtab") == 0) {
        ret = ctm_clarify_batch_backtab(&io);
    } else if (strcmp(func, "command_palette_backspace") == 0) {
        ret = ctm_command_palette_backspace(&io);
    } else if (strcmp(func, "command_palette_down") == 0) {
        ret = ctm_command_palette_down(&io);
    } else if (strcmp(func, "command_palette_up") == 0) {
        ret = ctm_command_palette_up(&io);
    } else if (strcmp(func, "command_palette_enter") == 0) {
        ret = ctm_command_palette_enter(&io);
    } else if (strcmp(func, "command_palette_escape") == 0) {
        ret = ctm_command_palette_escape(&io);
    } else if (strcmp(func, "slash_confirm_down") == 0) {
        ret = ctm_slash_confirm_down(&io);
    } else if (strcmp(func, "slash_confirm_up") == 0) {
        ret = ctm_slash_confirm_up(&io);
    } else if (strcmp(func, "approval_down") == 0) {
        ret = ctm_approval_down(&io);
    } else if (strcmp(func, "approval_up") == 0) {
        ret = ctm_approval_up(&io);
    } else if (strcmp(func, "handle_double_escape") == 0) {
        ret = ctm_handle_double_escape(&io);
    } else if (strcmp(func, "handle_alt_v") == 0) {
        ret = ctm_handle_alt_v(&io);
    } else if (strcmp(func, "handle_ctrl_v") == 0) {
        ret = ctm_handle_ctrl_v(&io);
    } else if (strcmp(func, "handle_ctrl_l") == 0) {
        ret = ctm_handle_ctrl_l(&io);
    } else if (strcmp(func, "insert_newline") == 0) {
        ret = ctm_insert_newline(&io);
    } else if (strcmp(func, "handle_open_in_editor") == 0) {
        ret = ctm_handle_open_in_editor(&io);
    } else if (strcmp(func, "recall_without_recollapse") == 0) {
        ret = ctm_recall_without_recollapse(&io);
    } else if (strcmp(func, "history_up") == 0) {
        ret = ctm_history_up(&io, json_get_num(in, "arg", 1));
    } else if (strcmp(func, "history_down") == 0) {
        ret = ctm_history_down(&io, json_get_num(in, "arg", 1));
    } else if (strcmp(func, "wake_startup") == 0) {
        ret = ctm_wake_startup(&io);
    } else if (strcmp(func, "handle_ignored_terminal_sequence") == 0) {
        ret = ctm_handle_ignored_terminal_sequence(&io, json_obj_get(in, "key_sequence"));
    } else if (strcmp(func, "stash_panel_up") == 0) {
        ret = ctm_stash_panel_up(&io);
    } else if (strcmp(func, "stash_panel_down") == 0) {
        ret = ctm_stash_panel_down(&io);
    } else if (strcmp(func, "stash_panel_delete") == 0) {
        ret = ctm_stash_panel_delete(&io);
    } else if (strcmp(func, "stash_panel_close") == 0) {
        ret = ctm_stash_panel_close(&io);
    } else if (strcmp(func, "restore_stash_payload") == 0) {
        ret = ctm_restore_stash_payload(&io, json_obj_get(in, "payload"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }
#undef ARGS0

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    if (state_out) {
        json_set(out, "state", state_out);   /* ownership moves into out */
        state_out = NULL;
    } else {
        json_set(out, "state", json_copy(state));
    }
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
        json_t *out = ctm_run_case(func, in);
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