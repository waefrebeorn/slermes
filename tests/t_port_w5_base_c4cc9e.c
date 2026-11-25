#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_base_c4cc9e.h"

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
        if (!strcmp(func, "hostname")) {
            char *p = w5_base_c4cc9e_get_hostname(json_get_str(in, "hostname", ""),
                                                 json_get_str(in, "base_url", ""));
            out = p ? json_string(p) : json_null();
            free(p);
        } else if (!strcmp(func, "max_tokens")) {
            json_t *v = json_obj_get(in, "default_max_tokens");
            out = w5_base_c4cc9e_get_max_tokens(
                    (!v || v->type == JSON_NULL) ? 1 : 0,
                    (v && v->type == JSON_STRING) ? v->str_val : "0");
        } else if (!strcmp(func, "api_kwargs")) {
            out = w5_base_c4cc9e_api_kwargs();
        } else if (!strcmp(func, "prepare_messages")) {
            out = w5_base_c4cc9e_prepare_messages(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "reasoning_efforts")) {
            out = w5_base_c4cc9e_supported_reasoning_efforts();
        } else if (!strcmp(func, "vision_model")) {
            out = w5_base_c4cc9e_default_vision_model();
        } else if (!strcmp(func, "aux_model")) {
            out = w5_base_c4cc9e_resolve_aux_model();
        } else if (!strcmp(func, "extra_body")) {
            out = w5_base_c4cc9e_build_extra_body();
        } else if (!strcmp(func, "reasoning_config")) {
            out = w5_base_c4cc9e_default_reasoning_config();
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
