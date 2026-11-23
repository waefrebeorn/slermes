#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_auxiliaryhe_a7faf2.h"

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
        if (!strcmp(func, "quarantine_ttl")) {
            out = w8_auxiliaryhe_a7faf2_quarantine_ttl(json_get_str(in, "reason", ""));
        } else if (!strcmp(func, "custom_health_base_url")) {
            out = w8_auxiliaryhe_a7faf2_custom_health_base_url(
                json_get_str(in, "provider", ""),
                json_get_str(in, "explicit", ""),
                json_get_str(in, "label", ""),
                json_get_str(in, "current_custom", ""),
                json_get_bool(in, "resolves", false) ? 1 : 0,
                json_obj_get(in, "named_entry"));
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
