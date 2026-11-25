/* JSON-lines harness for the w21_adapteropu_d51886 gate
 * (plugins/platforms/discord/adapter_opus.py).
 *
 * funcs:
 *   bundled {"opus_file","platform","ptr_bits","files"}
 *   plan    {...,"find_library","already_loaded","probed","load_script"}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_adapteropu_d51886.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int int_key(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? (int)v->num_val : def;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bundled")) {
            out = w21_adapteropu_d51886_bundled_opus_path(json_obj_get(in, "opus_file"),
                                                          json_obj_get(in, "platform"),
                                                          int_key(in, "ptr_bits", 64),
                                                          json_obj_get(in, "files"));
        } else if (!strcmp(func, "plan")) {
            out = w21_adapteropu_d51886_load_opus_codec_plan(in);
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
