/* JSON-lines harness for the w32_pluginvali_b2daaa_p2 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_pluginvali_b2daaa_p2.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "render")) {
            char *s = w32_pluginvali_b2daaa_p2_render(json_obj_get(in, "chain"));
            out = (s ? json_string(s) : json_null());
            free(s);
        } else if (!strcmp(func, "dotted")) {
            char *s = w32_pluginvali_b2daaa_p2_dotted(json_obj_get(in, "chain"));
            out = (s ? json_string(s) : json_null());
            free(s);
        } else if (!strcmp(func, "has_private_segment")) {
            out = json_bool(w32_pluginvali_b2daaa_p2_has_private_segment(json_obj_get(in, "chain")));
        } else if (!strcmp(func, "has_mutator_call")) {
            out = json_bool(w32_pluginvali_b2daaa_p2_has_mutator_call(json_obj_get(in, "chain")));
        } else if (!strcmp(func, "func_key")) {
            char *s = w32_pluginvali_b2daaa_p2_func_key(json_get_str(in, "dotted", ""),
                                                        json_obj_get(in, "aliases"));
            out = (s ? json_string(s) : json_null());
            free(s);
        } else if (!strcmp(func, "helper_indices")) {
            out = w32_pluginvali_b2daaa_p2_helper_indices(json_obj_get(in, "call"),
                                                          json_obj_get(in, "table"),
                                                          json_obj_get(in, "aliases"),
                                                          json_obj_get(in, "containers"),
                                                          json_obj_get(in, "rebound"));
        } else if (!strcmp(func, "report")) {
            out = w32_pluginvali_b2daaa_p2_report(json_obj_get(in, "hits"));
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
