#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_transcripti_1ec31e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "error_result")) {
            out = w14_transcripti_1ec31e_error_result(json_obj_get(in, "error"),
                        json_obj_get(in, "extra"));
        } else if (!strcmp(func, "ok_result")) {
            out = w14_transcripti_1ec31e_ok_result(json_obj_get(in, "transcript"),
                        json_obj_get(in, "provider"));
        } else if (!strcmp(func, "normalize_xai")) {
            out = w14_transcripti_1ec31e_normalize_xai_stt_model(json_obj_get(in, "model"),
                        json_obj_get(in, "env_value"));
        } else if (!strcmp(func, "config_number")) {
            out = w14_transcripti_1ec31e_config_number(json_obj_get(in, "cfg"),
                        json_get_str(in, "key", ""), json_obj_get(in, "default"),
                        json_get_str(in, "cast", "float"));
        } else if (!strcmp(func, "process_error_detail")) {
            out = w14_transcripti_1ec31e_process_error_detail(json_obj_get(in, "stderr"),
                        json_obj_get(in, "stdout"), json_obj_get(in, "exc_str"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
