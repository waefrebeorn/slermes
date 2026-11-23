#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_isolatedsc_454cd5.h"

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
        if (!strcmp(func, "filter")) {
            out = w12_isolatedsc_454cd5_filter(json_obj_get(in, "scope"),
                                               json_obj_get(in, "items"),
                                               json_get_str(in, "key", ""));
        } else if (!strcmp(func, "record")) {
            out = w12_isolatedsc_454cd5_record(
                    json_get_bool(in, "isolated", 0) ? 1 : 0,
                    json_get_bool(in, "desktop_owned", 0) ? 1 : 0,
                    json_get_str(in, "home", ""),
                    json_get_str(in, "default_root", ""),
                    json_get_str(in, "profile", ""));
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
