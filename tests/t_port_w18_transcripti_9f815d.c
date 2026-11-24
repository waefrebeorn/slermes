/* t_port_w18_transcripti_9f815d.c — harness for src/tools/transcription_streaming.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_transcripti_9f815d.h"

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
        if (!strcmp(func, "resample")) {
            char *hex = w18_transcripti_9f815d_resample(
                (int)json_get_num(in, "src_rate", 16000),
                (int)json_get_num(in, "dst_rate", 16000),
                json_obj_get(in, "chunks"));
            out = json_string(hex ? hex : "");
            free(hex);
        } else if (!strcmp(func, "error")) {
            out = w18_transcripti_9f815d_error(json_get_str(in, "provider", "unknown"),
                                               json_get_str(in, "message", ""));
        } else
            out = json_string("UNKNOWN_FUNC");
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
