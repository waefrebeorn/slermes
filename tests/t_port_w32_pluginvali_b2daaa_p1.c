/* JSON-lines harness for the w32_pluginvali_b2daaa_p1 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_pluginvali_b2daaa_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_test_file")) {
            out = json_bool(w32_pluginvali_b2daaa_p1_is_test_file(json_get_str(in, "rel", "")));
        } else if (!strcmp(func, "should_skip")) {
            out = json_bool(w32_pluginvali_b2daaa_p1_should_skip(json_get_str(in, "rel", "")));
        } else if (!strcmp(func, "core_module_name")) {
            out = json_bool(w32_pluginvali_b2daaa_p1_core_module_name(json_obj_get(in, "name"),
                                                                      json_obj_get(in, "roots")));
        } else if (!strcmp(func, "core_roots")) {
            out = w32_pluginvali_b2daaa_p1_core_roots(json_obj_get(in, "packages"),
                                                      json_obj_get(in, "modules"),
                                                      json_obj_get(in, "moves"),
                                                      json_obj_get(in, "local"));
        } else if (!strcmp(func, "lookup_is_core")) {
            out = json_bool(w32_pluginvali_b2daaa_p1_lookup_is_core(json_obj_get(in, "spec"),
                                                                    json_obj_get(in, "constants"),
                                                                    json_obj_get(in, "roots")));
        } else if (!strcmp(func, "is_container")) {
            out = json_bool(w32_pluginvali_b2daaa_p1_is_container(json_obj_get(in, "spec"),
                                                                  json_obj_get(in, "rebound")));
        } else if (!strcmp(func, "bound_names")) {
            out = w32_pluginvali_b2daaa_p1_bound_names(json_obj_get(in, "targets"));
        } else if (!strcmp(func, "module_constants")) {
            out = w32_pluginvali_b2daaa_p1_module_constants();
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
