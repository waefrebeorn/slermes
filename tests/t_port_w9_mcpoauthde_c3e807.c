#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_mcpoauthde_c3e807.h"

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
        if (!strcmp(func, "positive_seconds")) {
            out = w9_mcpoauthde_c3e807_positive_seconds(json_obj_get(in, "value"), json_get_str(in, "label", "interval"));
        } else if (!strcmp(func, "payload")) {
            out = w9_mcpoauthde_c3e807_payload((int)json_get_num(in, "status", 0), json_obj_get(in, "data"),
                              json_get_bool(in, "bad_json", false) ? 1 : 0,
                              json_get_str(in, "label", "payload"));
        } else if (!strcmp(func, "constants")) {
            out = w9_mcpoauthde_c3e807_constants();
        } else if (!strcmp(func, "device_grant_supported")) {
            out = json_bool(w9_mcpoauthde_c3e807_device_grant_supported(json_obj_get(in, "grants")) ? true : false);
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
