#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_transcripti_f10ae7.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "normalize_language")) {
            out = w13_transcripti_f10ae7_normalize_local_stt_language(
                json_obj_get(in, "language"), json_obj_get(in, "supported"));
        } else if (!strcmp(func, "normalize_model")) {
            out = w13_transcripti_f10ae7_normalize_local_model(json_obj_get(in, "model"));
        } else if (!strcmp(func, "idle_unload")) {
            out = w13_transcripti_f10ae7_get_idle_unload_seconds(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "cuda_error")) {
            out = w13_transcripti_f10ae7_looks_like_cuda_lib_error(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "confidence_thresholds")) {
            out = w13_transcripti_f10ae7_confidence_thresholds(json_obj_get(in, "cfg"));
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
