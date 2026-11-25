#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_authnous_65e5cf.h"

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
        if (!strcmp(func, "scope_values")) {
            out = w7_authnous_65e5cf_scope_values(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "model_priority")) {
            out = w7_authnous_65e5cf_model_priority(json_get_str(in, "mid", ""));
        } else if (!strcmp(func, "validate_url")) {
            out = w7_authnous_65e5cf_validate_inference_url(
                json_get_str(in, "url", ""), json_get_str(in, "override", ""));
        } else if (!strcmp(func, "portal_host")) {
            out = w7_authnous_65e5cf_portal_host(json_get_str(in, "url", ""));
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
