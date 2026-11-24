/* JSON-lines harness for the w21_search_eff38d gate (connector search leg).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_search_eff38d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "format_name")) {
            char *s = w21_search_eff38d_format_name(str_or(in, "connector", ""),
                                                    str_or(in, "tool", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "vendor_candidates")) {
            out = w21_search_eff38d_vendor_candidates(str_or(in, "connector", ""),
                                                      str_or(in, "tool", ""));
        } else if (!strcmp(func, "is_connector_name")) {
            out = json_bool(w21_search_eff38d_is_connector_name(json_obj_get(in, "name")));
        } else if (!strcmp(func, "model_parameters")) {
            out = w21_search_eff38d_model_parameters(json_obj_get(in, "input_schema"));
        } else if (!strcmp(func, "in_scope")) {
            out = json_bool(w21_search_eff38d_connections_in_scope(json_obj_get(in, "tool_defs")));
        } else if (!strcmp(func, "unavailable")) {
            out = w21_search_eff38d_unavailable(in);
        } else if (!strcmp(func, "entries_by_group")) {
            out = w21_search_eff38d_entries_by_group(in);
        } else if (!strcmp(func, "remote_schemas")) {
            out = w21_search_eff38d_remote_schemas(in);
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
