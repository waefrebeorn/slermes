#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_clituimix_9a2c8a.h"

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
        if (!strcmp(func, "num_prefix")) {
            out = w10_clituimix_9a2c8a_num_prefix(json_obj_get(in, "i"));
        } else if (!strcmp(func, "choice")) {
            out = w10_clituimix_9a2c8a_choice_label(json_obj_get(in, "choice"));
        } else if (!strcmp(func, "tn")) {
            out = w10_clituimix_9a2c8a_tn(json_obj_get(in, "key"), json_obj_get(in, "count"));
        } else if (!strcmp(func, "audio_bar")) {
            out = w10_clituimix_9a2c8a_audio_bar(json_obj_get(in, "rms"));
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
