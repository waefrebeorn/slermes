#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_mcptoolcon_4e294e.h"

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
        if (!strcmp(func, "is_reserved_meta_key")) {
            out = json_bool(w9_mcptoolcon_4e294e_is_reserved_meta_key(json_get_str(in, "key", "")) ? true : false);
        } else if (!strcmp(func, "strip_reserved_meta")) {
            out = w9_mcptoolcon_4e294e_strip_reserved_meta(json_obj_get(in, "meta"));
        } else if (!strcmp(func, "base_mime")) {
            char *s = w9_mcptoolcon_4e294e_base_mime(json_obj_get(in, "mime"));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "image_extension")) {
            char *s = w9_mcptoolcon_4e294e_image_extension(json_obj_get(in, "mime"), json_get_str(in, "ext", ""));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "resource_filename")) {
            char *s = w9_mcptoolcon_4e294e_resource_filename(json_get_str(in, "uri", ""), json_obj_get(in, "mime"),
                                            json_get_str(in, "ext", ""));
            out = s ? json_string(s) : json_null(); free(s);
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
