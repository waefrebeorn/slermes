/* JSON-lines harness for the w24_names_184724 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_names_184724.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_connector_name")) {
            out = w24_names_184724_is_connector_name(json_obj_get(in, "name"));
        } else if (!strcmp(func, "parse_connector_name")) {
            out = w24_names_184724_parse_connector_name(json_obj_get(in, "name"));
        } else if (!strcmp(func, "format_connector_name")) {
            out = w24_names_184724_format_connector_name(json_obj_get(in, "connector"),
                                                         json_obj_get(in, "tool"));
        } else if (!strcmp(func, "vendor_slug_candidates")) {
            out = w24_names_184724_vendor_slug_candidates(json_obj_get(in, "connector"),
                                                          json_obj_get(in, "tool"));
        } else if (!strcmp(func, "constants")) {
            out = w24_names_184724_constants();
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
