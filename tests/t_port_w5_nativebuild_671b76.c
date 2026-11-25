#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_nativebuild_671b76.h"

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
        int status = 0;
        if (!strcmp(func, "prepare_validate")) {
            out = w5_nativebuild_671b76_prepare_validate(json_obj_get(in, "prepared"), &status);
            if (status == 2 && !out) out = json_string("ValueError");
        } else if (!strcmp(func, "source_build_environment")) {
            out = w5_nativebuild_671b76_source_build_environment_ex(
                json_get_str(in, "target", ""),
                json_get_bool(in, "provider_present", false) ? 1 : 0,
                json_obj_get(in, "prepared"), json_obj_get(in, "bridged"), &status);
            if (status == 1 && !out) out = json_null();
            else if (status == 2 && !out) out = json_string("ValueError");
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
