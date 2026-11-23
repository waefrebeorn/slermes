/* t_port_w10_ttstoolde_5650bd.c -- JSON-lines harness for src/tools/tts_tool_delivery.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_ttstoolde_5650bd.h"

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
        if (!strcmp(func, "section")) {
            out = w10_ttstoolde_5650bd_section(json_obj_get(in, "cfg"), json_get_str(in, "key", ""));
        } else if (!strcmp(func, "posint")) {
            out = w10_ttstoolde_5650bd_positive_int(json_obj_get(in, "value"));
        } else if (!strcmp(func, "maxlen")) {
            out = w10_ttstoolde_5650bd_resolve_max_text_length(json_get_str(in, "provider", ""),
                    json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "profile")) {
            out = w10_ttstoolde_5650bd_resolve_audio_delivery_profile(json_get_str(in, "platform", ""),
                    json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "pack")) {
            out = w10_ttstoolde_5650bd_pack_under_cap(json_obj_get(in, "pieces"),
                    (int)json_get_num(in, "max_chars", 10.0),
                    json_get_bool(in, "slice_oversized", false) ? 1 : 0);
        } else if (!strcmp(func, "splitlong")) {
            out = w10_ttstoolde_5650bd_split_oversized_sentence(json_get_str(in, "sentence", ""),
                    (int)json_get_num(in, "max_chars", 10.0));
        } else if (!strcmp(func, "splittext")) {
            out = w10_ttstoolde_5650bd_split_text_for_tts(json_get_str(in, "text", ""),
                    (int)json_get_num(in, "max_chars", 4000.0));
        } else if (!strcmp(func, "sidecar")) {
            out = w10_ttstoolde_5650bd_wav_sidecar_path(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "wav")) {
            out = w10_ttstoolde_5650bd_wrap_pcm_as_wav(json_obj_get(in, "pcm"),
                    (int)json_get_num(in, "rate", 24000.0),
                    (int)json_get_num(in, "channels", 1.0),
                    (int)json_get_num(in, "width", 2.0));
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
