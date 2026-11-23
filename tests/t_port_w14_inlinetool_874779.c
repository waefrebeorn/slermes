#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_inlinetool_874779.h"

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
        if (!strcmp(func, "tool_hook_ids")) {
            out = w14_inlinetool_874779_tool_hook_ids(
                json_get_str(in, "task_id", ""), json_get_str(in, "session_id", ""),
                json_get_str(in, "tool_call_id", ""), json_get_str(in, "turn_id", ""),
                json_get_str(in, "api_request_id", ""), json_obj_get(in, "conversation_id"));
        } else if (!strcmp(func, "is_context_engine_tool")) {
            out = json_bool((bool)w14_inlinetool_874779_is_context_engine_tool(
                json_obj_get(in, "names"), json_get_str(in, "function_name", "")));
        } else if (!strcmp(func, "is_memory_provider_tool")) {
            out = json_bool((bool)w14_inlinetool_874779_is_memory_provider_tool(
                json_get_bool(in, "has_manager", false) ? 1 : 0,
                json_obj_get(in, "tool_names"), json_get_str(in, "function_name", "")));
        } else if (!strcmp(func, "pre_registry_route")) {
            const char *r = w14_inlinetool_874779_pre_registry_route(
                json_get_str(in, "function_name", ""),
                json_obj_get(in, "legacy_aliases"), json_obj_get(in, "bridge_names"),
                json_obj_get(in, "agent_loop_tools"), json_obj_get(in, "connector_names"),
                json_obj_get(in, "context_engine_names"), json_obj_get(in, "memory_provider_names"),
                json_get_bool(in, "has_agent", false) ? 1 : 0);
            out = r ? json_string(r) : json_null();
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
