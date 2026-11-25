/* t_port_w11_ttstoolpr_0ad803.c - harness for tools/tts_tool_providers.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_ttstoolpr_0ad803.h"

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
        if (!strcmp(func, "config_bool")) {
            out = w11_ttstoolpr_0ad803_config_bool(json_obj_get(in, "value"),
                    json_get_bool(in, "default", false) ? 1 : 0);
        } else if (!strcmp(func, "response_format")) {
            out = w11_ttstoolpr_0ad803_response_format(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "clamped_number")) {
            out = w11_ttstoolpr_0ad803_clamped_number(json_obj_get(in, "raw"),
                    json_get_str(in, "cast", "float"),
                    json_get_num(in, "lo", 0.0), json_get_num(in, "hi", 1.0));
        } else if (!strcmp(func, "gemini_tags")) {
            out = w11_ttstoolpr_0ad803_gemini_audio_tags(json_obj_get(in, "gemini_config"),
                    json_get_str(in, "model", ""));
        } else if (!strcmp(func, "compose")) {
            out = w11_ttstoolpr_0ad803_compose_gemini_prompt(json_get_str(in, "text", ""),
                    json_get_str(in, "persona_prompt", ""));
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
