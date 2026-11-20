/* t_port_cli_tui_mixin_flow2.c — harness for the hermes_cli/cli_tui_mixin.py
 * unclaimed-remainder FLOW port (src/cli/cli_tui_mixin_flow2.c).
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
 * The state mirror is emitted EVEN ON A RAISE (partial mutation history).
 */
#include "hermes_json.h"
#include "cli/cli_tui_mixin_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} ctm2_script_ctx_t;

static json_t *ctm2_script_next(ctm2_script_ctx_t *sc, const char *name) {
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

static void ctm2_apply_state_effects(json_t *state, json_t *result) {
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

static json_t *ctm2_dispatch(ctm2_io_t *io, const char *name, json_t *args) {
    (void)args;
    ctm2_script_ctx_t *sc = (ctm2_script_ctx_t *)io->ctx;
    json_t *entry = ctm2_script_next(sc, name);
    if (!entry) return json_null();
    if (entry->type == JSON_OBJECT) {
        ctm2_apply_state_effects(io->state, entry);
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

static json_t *ctm2_run_case(const char *func, json_t *in) {
    ctm2_io_t io;
    ctm2_script_ctx_t sc;
    json_t *state = json_obj_get(in, "state");
    state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
    io.call = ctm2_dispatch;
    io.trace = json_array();
    io.state = state;
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    json_t *ret = NULL;
    long long idx = (long long)json_get_num(in, "idx", 0);
    const char *ch = json_get_str(in, "ch", "");
    const char *platform = json_get_str(in, "platform", "win32");

    if (strcmp(func, "get_tui_prompt_symbols") == 0) {
        ret = ctm2_get_tui_prompt_symbols(&io);
    } else if (strcmp(func, "get_tui_prompt_fragments") == 0) {
        ret = ctm2_get_tui_prompt_fragments(&io);
    } else if (strcmp(func, "build_tui_style_dict") == 0) {
        ret = ctm2_build_tui_style_dict(&io);
    } else if (strcmp(func, "apply_tui_skin_style") == 0) {
        ret = ctm2_apply_tui_skin_style(&io);
    } else if (strcmp(func, "get_extra_tui_widgets") == 0) {
        ret = ctm2_get_extra_tui_widgets(&io);
    } else if (strcmp(func, "tui_hint_text") == 0) {
        ret = ctm2_tui_hint_text(&io);
    } else if (strcmp(func, "tui_placeholder_text") == 0) {
        ret = ctm2_tui_placeholder_text(&io);
    } else if (strcmp(func, "get_stash_panel_display_fragments") == 0) {
        ret = ctm2_get_stash_panel_display_fragments(&io);
    } else if (strcmp(func, "tui_image_bar_fragments") == 0) {
        ret = ctm2_tui_image_bar_fragments(&io);
    } else if (strcmp(func, "tui_voice_status_fragments") == 0) {
        ret = ctm2_tui_voice_status_fragments(&io);
    } else if (strcmp(func, "tui_spinner_text") == 0) {
        ret = ctm2_tui_spinner_text(&io);
    } else if (strcmp(func, "tui_spinner_height") == 0) {
        ret = ctm2_tui_spinner_height(&io);
    } else if (strcmp(func, "tui_hint_height") == 0) {
        ret = ctm2_tui_hint_height(&io);
    } else if (strcmp(func, "handle_tab") == 0) {
        ret = ctm2_handle_tab(&io);
    } else if (strcmp(func, "open_command_palette") == 0) {
        ret = ctm2_open_command_palette(&io);
    } else if (strcmp(func, "handle_stash_panel_restore") == 0) {
        ret = ctm2_handle_stash_panel_restore(&io);
    } else if (strcmp(func, "make_clarify_number_handler") == 0) {
        ret = ctm2_make_clarify_number_handler(&io, idx);
    } else if (strcmp(func, "make_model_filter_char_handler") == 0) {
        ret = ctm2_make_model_filter_char_handler(&io, ch);
    } else if (strcmp(func, "make_palette_char_handler") == 0) {
        ret = ctm2_make_palette_char_handler(&io, ch);
    } else if (strcmp(func, "make_approval_number_handler") == 0) {
        ret = ctm2_make_approval_number_handler(&io, idx);
    } else if (strcmp(func, "make_slash_confirm_number_handler") == 0) {
        ret = ctm2_make_slash_confirm_number_handler(&io, idx);
    } else if (strcmp(func, "handle_ctrl_z") == 0) {
        ret = ctm2_handle_ctrl_z(&io, platform);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", json_copy(state));   /* emitted even on a raise */
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
        json_t *out = ctm2_run_case(func, in);
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