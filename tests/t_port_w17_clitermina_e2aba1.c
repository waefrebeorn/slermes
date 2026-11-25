#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_clitermina_e2aba1.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        json_t *env = json_obj_get(in, "env");
        json_t *config = json_obj_get(in, "config");
        if (!json_is_object(env)) env = NULL;
        if (!json_is_object(config)) config = NULL;
        const char *text = json_get_str(in, "text", "");
        const char *raw = json_get_str(in, "raw", "");
        if (!strcmp(func, "is_ghostty")) {
            out = json_bool(w17_clitermina_e2aba1_is_ghostty(env) ? true : false);
        } else if (!strcmp(func, "supports_enter")) {
            out = json_bool(w17_clitermina_e2aba1_supports_extended_enter_keys(env) ? true : false);
        } else if (!strcmp(func, "multiline")) {
            out = json_bool(w17_clitermina_e2aba1_multiline_shortcuts_enabled(config) ? true : false);
        } else if (!strcmp(func, "backslash_check")) {
            out = json_bool(w17_clitermina_e2aba1_is_backslash_line_continuation(text) ? true : false);
        } else if (!strcmp(func, "backslash_apply")) {
            out = w17_clitermina_e2aba1_apply_backslash_line_continuation(text);
        } else if (!strcmp(func, "clipboard")) {
            out = json_bool(w17_clitermina_e2aba1_should_auto_attach_clipboard_image(text) ? true : false);
        } else if (!strcmp(func, "statusbar")) {
            out = json_bool(w17_clitermina_e2aba1_status_bar_visible(config) ? true : false);
        } else if (!strcmp(func, "split_path")) {
            out = w17_clitermina_e2aba1_split_path_input(raw);
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
