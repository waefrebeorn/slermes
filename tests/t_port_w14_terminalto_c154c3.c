#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_terminalto_c154c3.h"

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
        if (!strcmp(func, "validate_workdir")) {
            out = w14_terminalto_c154c3_validate_workdir(json_obj_get(in, "workdir"));
        } else if (!strcmp(func, "preview")) {
            out = w14_terminalto_c154c3_safe_command_preview(json_obj_get(in, "command"),
                        (int)json_get_num(in, "limit", 200));
        } else if (!strcmp(func, "blocked_json")) {
            out = w14_terminalto_c154c3_blocked_json(json_obj_get(in, "error"),
                        json_obj_get(in, "status"));
        } else if (!strcmp(func, "help_version")) {
            out = w14_terminalto_c154c3_looks_like_help_or_version_command(json_obj_get(in, "command"));
        } else if (!strcmp(func, "strip_quotes")) {
            out = w14_terminalto_c154c3_strip_quotes(json_obj_get(in, "command"));
        } else if (!strcmp(func, "strip_trailing_amp")) {
            out = w14_terminalto_c154c3_strip_trailing_amp(json_obj_get(in, "command"));
        } else if (!strcmp(func, "has_flag")) {
            out = w14_terminalto_c154c3_has_flag(json_obj_get(in, "args"),
                        json_get_str(in, "short", ""), json_obj_get(in, "long"));
        } else if (!strcmp(func, "is_data")) {
            out = w14_terminalto_c154c3_is_data_command(json_obj_get(in, "word"),
                        json_obj_get(in, "args"));
        } else if (!strcmp(func, "returns_promptly")) {
            out = w14_terminalto_c154c3_returns_promptly(json_obj_get(in, "word"),
                        json_obj_get(in, "args"));
        } else if (!strcmp(func, "command_word")) {
            out = w14_terminalto_c154c3_command_word(json_obj_get(in, "tokens"));
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
