/* Port gate harness for agent/terminal_image.py (gate w7_terminalima_83613a). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_terminalima_83613a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "kitty_color_hex")) {
            char *s = w7_terminalima_83613a_kitty_color_hex(json_get_str(in, "image_id", "0"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "cell_box")) {
            out = w7_terminalima_83613a_cell_box(json_get_str(in, "width", "0"),
                                                 json_get_str(in, "height", "0"));
        } else if (!strcmp(func, "kitty_apc")) {
            char *s = w7_terminalima_83613a_kitty_apc(json_get_str(in, "ctrl", ""),
                                                      json_get_str(in, "data", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "grid_for")) {
            out = w7_terminalima_83613a_grid_for(json_get_str(in, "width", "0"),
                                                 json_get_str(in, "height", "0"),
                                                 json_get_str(in, "max_cols", "0"),
                                                 json_get_str(in, "max_rows", "0"));
        } else if (!strcmp(func, "kitty_placeholder_rows")) {
            out = w7_terminalima_83613a_kitty_placeholder_rows(json_get_str(in, "cols", "0"),
                                                               json_get_str(in, "rows", "0"));
        } else if (!strcmp(func, "detect_graphics")) {
            out = json_string(w7_terminalima_83613a_detect_graphics(
                json_get_str(in, "TERM", ""), json_get_str(in, "TERM_PROGRAM", ""),
                json_get_str(in, "KITTY_WINDOW_ID", ""), json_get_str(in, "ITERM_SESSION_ID", ""),
                json_get_str(in, "WEZTERM_PANE", "")));
        } else if (!strcmp(func, "supports_kitty_placeholders")) {
            out = json_bool(w7_terminalima_83613a_supports_kitty_placeholders(
                json_get_str(in, "TERM", ""), json_get_str(in, "TERM_PROGRAM", ""),
                json_get_str(in, "KITTY_WINDOW_ID", ""), json_get_str(in, "ITERM_SESSION_ID", ""),
                json_get_str(in, "WEZTERM_PANE", "")));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
