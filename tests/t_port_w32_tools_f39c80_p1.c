/* JSON-lines harness for the w32_tools_f39c80_p1 gate (terminal backends +
 * toolset model catalog).  One request object per stdin line; one response
 * line carrying the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_tools_f39c80_p1.h"

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
        if (!strcmp(func, "cfg_value")) {
            out = w32_tools_f39c80_p1_cfg_value(json_obj_get(in, "terminal_cfg"),
                                                json_obj_get(in, "cfg_key"),
                                                json_obj_get(in, "env_var"),
                                                json_obj_get(in, "env_map"));
        } else if (!strcmp(func, "probe_ssh")) {
            out = w32_tools_f39c80_p1_probe_ssh(json_obj_get(in, "terminal_cfg"),
                                                json_obj_get(in, "env_map"));
        } else if (!strcmp(func, "probe_modal")) {
            out = w32_tools_f39c80_p1_probe_modal(json_obj_get(in, "has_direct"),
                                                  json_obj_get(in, "direct_raises"),
                                                  json_obj_get(in, "env_map"));
        } else if (!strcmp(func, "probe_daytona")) {
            out = w32_tools_f39c80_p1_probe_daytona(json_obj_get(in, "env_map"));
        } else if (!strcmp(func, "probe_docker")) {
            out = w32_tools_f39c80_p1_probe_docker(json_obj_get(in, "state"));
        } else if (!strcmp(func, "backends_payload")) {
            out = w32_tools_f39c80_p1_backends_payload(json_obj_get(in, "rows"),
                                                       json_obj_get(in, "config"),
                                                       json_obj_get(in, "probes"));
        } else if (!strcmp(func, "select_backend")) {
            out = w32_tools_f39c80_p1_select_backend(json_obj_get(in, "rows"),
                                                     json_obj_get(in, "backend"));
        } else if (!strcmp(func, "no_models")) {
            out = w32_tools_f39c80_p1_no_models(json_obj_get(in, "name"));
        } else if (!strcmp(func, "model_plugin")) {
            out = w32_tools_f39c80_p1_model_plugin(json_obj_get(in, "ts_key"),
                                                   json_obj_get(in, "row"));
        } else if (!strcmp(func, "catalog_section")) {
            out = w32_tools_f39c80_p1_catalog_section(json_obj_get(in, "name"));
        } else if (!strcmp(func, "models_payload")) {
            out = w32_tools_f39c80_p1_models_payload(json_obj_get(in, "name"),
                                                     json_obj_get(in, "config"),
                                                     json_obj_get(in, "row"),
                                                     json_obj_get(in, "catalog"),
                                                     json_obj_get(in, "default_model"));
        } else if (!strcmp(func, "model_select")) {
            out = w32_tools_f39c80_p1_model_select(json_obj_get(in, "name"),
                                                   json_obj_get(in, "model"),
                                                   json_obj_get(in, "config"),
                                                   json_obj_get(in, "row"),
                                                   json_obj_get(in, "catalog"));
        } else if (!strcmp(func, "provider_row")) {
            out = w32_tools_f39c80_p1_provider_row(json_obj_get(in, "rows"),
                                                   json_obj_get(in, "provider"),
                                                   json_obj_get(in, "active_flags"));
        } else if (!strcmp(func, "require_known")) {
            out = w32_tools_f39c80_p1_require_known(json_obj_get(in, "name"),
                                                    json_obj_get(in, "known"));
        } else if (!strcmp(func, "constants")) {
            out = w32_tools_f39c80_p1_constants();
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
