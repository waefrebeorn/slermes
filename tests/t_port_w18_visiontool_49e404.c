/* t_port_w18_visiontool_49e404.c — harness for src/tools/vision_tools_image_prep.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_visiontool_49e404.h"

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
        if (!strcmp(func, "unsupported_inline_image_media_type")) {
            const char *r = w18_visiontool_49e404_unsupported_inline_image_media_type(
                json_get_str(in, "url", ""));
            out = r ? json_string(r) : json_null();
            free((void *)r);
        } else if (!strcmp(func, "detect_image_mime_type_from_bytes")) {
            const char *r = w18_visiontool_49e404_detect_image_mime_type_from_bytes(
                json_get_str(in, "data_hex", ""),
                json_get_bool(in, "png_verify_ok", 1));
            out = r ? json_string(r) : json_null();
            free((void *)r);
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
