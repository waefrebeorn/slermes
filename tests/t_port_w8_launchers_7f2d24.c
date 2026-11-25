/* t_port_w8_launchers_7f2d24.c -- JSON-lines harness for src/hermes_cli/_launchers.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_launchers_7f2d24.h"

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
        if (!strcmp(func, "pin_default_home_flag")) {
            out = json_string(w8_launchers_7f2d24_pin_default_home_flag());
        } else if (!strcmp(func, "windows_bin_launchers")) {
            out = w8_launchers_7f2d24_windows_bin_launchers();
        } else if (!strcmp(func, "entry_points")) {
            out = w8_launchers_7f2d24_entry_points();
        } else if (!strcmp(func, "inline_string_literal")) {
            out = w8_launchers_7f2d24_inline_string_literal(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "first_token")) {
            out = w8_launchers_7f2d24_first_token(json_get_str(in, "line", ""));
        } else if (!strcmp(func, "merge_user_path")) {
            out = w8_launchers_7f2d24_merge_user_path(
                json_get_str(in, "existing", ""), json_get_str(in, "entry", ""));
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
