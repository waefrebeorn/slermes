#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_doctortools_317c87.h"

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
        if (!strcmp(func, "plural")) {
            out = w9_doctortools_317c87_plural((int)json_get_num(in, "n", 0));
        } else if (!strcmp(func, "setup_gated")) {
            out = json_bool(w9_doctortools_317c87_setup_gated(json_obj_get(in, "item")) ? true : false);
        } else if (!strcmp(func, "kanban_env_gate")) {
            out = json_bool(w9_doctortools_317c87_kanban_env_gate(
                      json_obj_get(in, "item"), json_obj_get(in, "env_value")) ? true : false);
        } else if (!strcmp(func, "tool_detail")) {
            out = w9_doctortools_317c87_tool_availability_detail(
                      json_get_str(in, "toolset", ""), json_obj_get(in, "env_value"));
        } else if (!strcmp(func, "apply_overrides")) {
            out = w9_doctortools_317c87_apply_overrides(json_obj_get(in, "available"),
                      json_obj_get(in, "unavailable"), json_obj_get(in, "env_value"));
        } else if (!strcmp(func, "missing_api")) {
            out = w9_doctortools_317c87_missing_api_key_toolsets(
                      json_obj_get(in, "unavailable"), json_obj_get(in, "enabled"));
        } else if (!strcmp(func, "safe_which")) {
            out = w9_doctortools_317c87_safe_which(json_get_str(in, "cmd", ""),
                      json_obj_get(in, "path_map"));
        } else if (!strcmp(func, "chromium_reason")) {
            out = w9_doctortools_317c87_chromium_not_needed_reason(
                      json_obj_get(in, "config"), json_obj_get(in, "cdp_env"));
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
