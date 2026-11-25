/* JSON-lines harness for the w32_tools_f39c80_p2 gate (toolsets matrix,
 * config panel, toggle / env / provider writes).  One request object per
 * stdin line; one response line carrying the computed "out" plus every input
 * key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_tools_f39c80_p2.h"

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
        if (!strcmp(func, "dict_section")) {
            out = w32_tools_f39c80_p2_dict_section(json_obj_get(in, "config"),
                                                   json_obj_get(in, "key"));
        } else if (!strcmp(func, "toolsets_rows")) {
            out = w32_tools_f39c80_p2_toolsets_rows(json_obj_get(in, "rows"),
                                                    json_obj_get(in, "platform_map"),
                                                    json_obj_get(in, "platform_tools"),
                                                    json_obj_get(in, "configured"),
                                                    json_obj_get(in, "gui_labels"),
                                                    json_obj_get(in, "platform_labels"),
                                                    json_obj_get(in, "config"),
                                                    json_obj_get(in, "config_only"),
                                                    json_obj_get(in, "tools_raw"),
                                                    json_obj_get(in, "tools_error"));
        } else if (!strcmp(func, "allowed_env_keys")) {
            out = w32_tools_f39c80_p2_allowed_env_keys(json_obj_get(in, "providers"));
        } else if (!strcmp(func, "save_env")) {
            out = w32_tools_f39c80_p2_save_env(json_obj_get(in, "name"),
                                               json_obj_get(in, "providers"),
                                               json_obj_get(in, "env_body"),
                                               json_obj_get(in, "env_set"),
                                               json_obj_get(in, "save_errors"),
                                               json_obj_get(in, "known"));
        } else if (!strcmp(func, "toggle")) {
            out = w32_tools_f39c80_p2_toggle(in);
        } else if (!strcmp(func, "toolset_config")) {
            out = w32_tools_f39c80_p2_toolset_config(in);
        } else if (!strcmp(func, "provider_capability")) {
            out = w32_tools_f39c80_p2_provider_capability(in);
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
