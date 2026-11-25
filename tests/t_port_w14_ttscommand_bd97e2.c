#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_ttscommand_bd97e2.h"

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
        const char *os_name = json_get_str(in, "os_name", "posix");
        json_t *out = NULL;
        if (!strcmp(func, "quote_context")) {
            out = w14_ttscommand_bd97e2_shell_quote_context(json_obj_get(in, "command_template"),
                        (int)json_get_num(in, "position", 0));
        } else if (!strcmp(func, "quote_placeholder")) {
            out = w14_ttscommand_bd97e2_quote_command_placeholder(json_obj_get(in, "value"),
                        json_obj_get(in, "quote_context"), os_name);
        } else if (!strcmp(func, "render")) {
            out = w14_ttscommand_bd97e2_render_command_template(json_obj_get(in, "command_template"),
                        json_obj_get(in, "placeholders"), os_name);
        } else if (!strcmp(func, "failure_detail")) {
            out = w14_ttscommand_bd97e2_command_failure_detail(json_obj_get(in, "stderr"),
                        json_obj_get(in, "stdout"));
        } else if (!strcmp(func, "provider_section")) {
            out = w14_ttscommand_bd97e2_get_provider_section(json_obj_get(in, "config"),
                        json_get_str(in, "name", "providers"));
        } else if (!strcmp(func, "is_command_config")) {
            out = w14_ttscommand_bd97e2_is_command_provider_config(json_obj_get(in, "config"));
        } else if (!strcmp(func, "named_config")) {
            out = w14_ttscommand_bd97e2_named_provider_config(json_obj_get(in, "config"),
                        json_get_str(in, "name", ""), json_obj_get(in, "builtins"));
        } else if (!strcmp(func, "resolve_config")) {
            out = w14_ttscommand_bd97e2_resolve_command_config(json_obj_get(in, "provider"),
                        json_obj_get(in, "config"), json_obj_get(in, "reserved"));
        } else if (!strcmp(func, "command_timeout")) {
            out = w14_ttscommand_bd97e2_command_timeout(json_obj_get(in, "config"),
                        json_get_num(in, "default", 120));
        } else if (!strcmp(func, "command_output_format")) {
            out = w14_ttscommand_bd97e2_command_output_format(json_obj_get(in, "config"),
                        json_obj_get(in, "formats"), json_get_str(in, "default", "mp3"));
        } else if (!strcmp(func, "tts_output_format")) {
            out = w14_ttscommand_bd97e2_get_command_tts_output_format(json_obj_get(in, "config"),
                        json_obj_get(in, "output_path"));
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
