#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_transcripti_f24ccf.h"

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
        if (!strcmp(func, "sdk_prompt_kwargs")) {
            out = w13_transcripti_f24ccf_sdk_prompt_kwargs(json_obj_get(in, "language"),
                                                           json_obj_get(in, "prompt"));
        } else if (!strcmp(func, "elevenlabs_error_detail")) {
            out = w13_transcripti_f24ccf_elevenlabs_error_detail(json_obj_get(in, "body"));
        } else if (!strcmp(func, "extract_transcript_text")) {
            out = w13_transcripti_f24ccf_extract_transcript_text(json_obj_get(in, "value"));
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
