/*
 * t_port_w7_ttstoolloc_4467f6.c — Oracle harness for tools/tts_tool_local.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_ttstoolloc_4467f6.h"

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
        json_t *out = NULL;
        const char *func;
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w7_ttstoolloc_4467f6_constants();
        } else if (!strcmp(func, "voice_default")) {
            out = w7_ttstoolloc_4467f6_voice_default(json_obj_get(in, "voice"));
        } else if (!strcmp(func, "cache")) {
            out = w7_ttstoolloc_4467f6_cache_get_or_load(json_obj_get(in, "entries"),
                json_get_str(in, "key", ""), json_obj_get(in, "load_value"),
                (long)json_get_num(in, "max", 3));
        } else if (!strcmp(func, "piper_cache_key")) {
            out = w7_ttstoolloc_4467f6_piper_cache_key(json_get_str(in, "model_path", ""),
                json_get_bool(in, "use_cuda", false) ? 1 : 0);
        } else if (!strcmp(func, "speaker_id")) {
            out = w7_ttstoolloc_4467f6_piper_speaker_id(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "advanced")) {
            out = w7_ttstoolloc_4467f6_has_advanced_knobs(json_obj_get(in, "keys"));
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        {
            size_t i;
            for (i = 0; i < json_object_size(in); i++) {
                const char *k = json_object_get_key_at(in, i);
                if (!k || !strcmp(k, "func")) continue;
                fputs(",", stdout);
                emit_key(k, json_copy(json_object_get_at(in, i)));
            }
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
