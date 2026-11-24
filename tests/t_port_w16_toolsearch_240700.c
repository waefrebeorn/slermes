#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_toolsearch_240700.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *sget(const json_t *o, const char *k) { return json_get_str(o, k, ""); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "schema_for_local"))
            out = w16_toolsearch_240700_schema_for_local_validation(json_obj_get(a, "node"));
        else if (!strcmp(func, "schema_has_external"))
            out = w16_toolsearch_240700_schema_has_external_ref(json_obj_get(a, "node"));
        else if (!strcmp(func, "normalize_entries"))
            out = w16_toolsearch_240700_normalize_tool_call_entries(
                json_obj_get(a, "args"), json_obj_get(a, "bridge_names"));
        else if (!strcmp(func, "local_batch_error"))
            out = w16_toolsearch_240700_local_batch_error(json_obj_get(a, "entries"));
        else if (!strcmp(func, "unavailable_in_session"))
            out = w16_toolsearch_240700_unavailable_in_session_reason(sget(a, "name"));
        else if (!strcmp(func, "session_unavailable")) {
            int cr = (json_get_bool(a, "in_core", false) && json_get_bool(a, "registered", false)) ? 1 : 0;
            out = w16_toolsearch_240700_session_unavailable_error(
                sget(a, "name"), json_obj_get(a, "session_names"), cr,
                sget(a, "out_of_scope"));
        } else if (!strcmp(func, "not_deferrable")) {
            int cor = json_get_bool(a, "core_or_registered", false) ? 1 : 0;
            out = w16_toolsearch_240700_not_deferrable_error(
                sget(a, "name"), json_obj_get(a, "session_names"), cor,
                json_obj_get(a, "all_tool_names"));
        } else out = json_string("UNKNOWN_FUNC");
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
