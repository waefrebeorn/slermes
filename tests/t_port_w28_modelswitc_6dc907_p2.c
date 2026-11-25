/* JSON-lines harness for the w28_modelswitc_6dc907_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_modelswitc_6dc907_p2.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "discover_models")) {
            out = w28ms_p2_discover_models(json_obj_get(in, "live_models"),
                                           json_obj_get(in, "opts"));
        } else if (!strcmp(func, "resolved_fast_probe")) {
            out = w28ms_p2_resolved_fast_probe(json_obj_get(in, "for_picker"),
                                               json_obj_get(in, "fast_custom_probe"));
        } else if (!strcmp(func, "can_probe_custom")) {
            out = w28ms_p2_can_probe_custom(json_obj_get(in, "probe_custom_providers"),
                                            json_obj_get(in, "probe_current_custom_provider"),
                                            json_obj_get(in, "row_is_current"));
        } else if (!strcmp(func, "endpoint_is_current")) {
            out = w28ms_p2_endpoint_is_current(json_obj_get(in, "slug"),
                                               json_obj_get(in, "aliases"),
                                               json_obj_get(in, "url_norm"),
                                               json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "record_section3_pair")) {
            out = w28ms_p2_record_section3_pair(json_obj_get(in, "name"),
                                                json_obj_get(in, "url_norm"));
        } else if (!strcmp(func, "add_builtin_row")) {
            out = w28ms_p2_add_builtin_row(json_obj_get(in, "req"));
        } else if (!strcmp(func, "add_endpoint_row")) {
            out = w28ms_p2_add_endpoint_row(json_obj_get(in, "req"));
        } else if (!strcmp(func, "discover_endpoint")) {
            out = w28ms_p2_discover_endpoint(json_obj_get(in, "req"));
        } else if (!strcmp(func, "lmstudio_row")) {
            out = w28ms_p2_lmstudio_row(json_obj_get(in, "req"));
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
