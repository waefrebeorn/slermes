/* Port gate harness for tools/voice_mode_transcript.py (gate w15_voicemode_7ad9c5). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_voicemode_7ad9c5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static char *str_or_empty(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? (char *)v->str_val : (char *)"";
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
        char *tmp = NULL;
        if (!strcmp(func, "hallucination")) {
            out = json_bool(w15_voicemode_7ad9c5_is_whisper_hallucination(str_or_empty(in, "text")));
        } else if (!strcmp(func, "normalize")) {
            tmp = w15_voicemode_7ad9c5_normalize_for_echo_compare(str_or_empty(in, "text"));
            out = json_string(tmp ? tmp : "");
        } else if (!strcmp(func, "echo")) {
            out = json_bool(w15_voicemode_7ad9c5_is_tts_echo(
                str_or_empty(in, "text"), str_or_empty(in, "spoken"),
                json_get_num(in, "threshold", 0.6)));
        } else if (!strcmp(func, "stop_phrase")) {
            out = json_bool(w15_voicemode_7ad9c5_is_voice_stop_phrase(
                str_or_empty(in, "text"), json_obj_get(in, "phrases")));
        } else if (!strcmp(func, "stop_hint")) {
            tmp = w15_voicemode_7ad9c5_voice_stop_hint(json_obj_get(in, "phrases"));
            out = json_string(tmp ? tmp : "");
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
