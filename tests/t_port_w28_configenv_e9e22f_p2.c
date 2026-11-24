/* JSON-lines harness for the w28_configenv_e9e22f_p2 gate (config_env payloads).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_configenv_e9e22f_p2.h"

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
        if (!strcmp(func, "strip_internal")) {
            out = w28_configenv_e9e22f_p2_strip_internal(json_obj_get(in, "config"));
        } else if (!strcmp(func, "endpoint_row")) {
            out = w28_configenv_e9e22f_p2_endpoint_row(json_obj_get(in, "args"));
        } else if (!strcmp(func, "custom_endpoint_response")) {
            out = w28_configenv_e9e22f_p2_custom_endpoint_response(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "pop_legacy_provider")) {
            out = w28_configenv_e9e22f_p2_pop_legacy_provider(json_obj_get(in, "cfg"),
                                                              json_obj_get(in, "provider_key"));
        } else if (!strcmp(func, "detach_main_model")) {
            out = w28_configenv_e9e22f_p2_detach_main_model(json_obj_get(in, "cfg"),
                                                            json_obj_get(in, "provider_key"),
                                                            json_obj_get(in, "entry"));
        } else if (!strcmp(func, "drop_stale_addresses")) {
            out = w28_configenv_e9e22f_p2_drop_stale_addresses(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "resolve_entry")) {
            out = w28_configenv_e9e22f_p2_resolve_entry(json_obj_get(in, "providers"),
                                                        json_obj_get(in, "endpoint_id"));
        } else if (!strcmp(func, "validate_endpoint")) {
            out = w28_configenv_e9e22f_p2_validate_endpoint(json_obj_get(in, "name"),
                                                            json_obj_get(in, "base_url"),
                                                            json_obj_get(in, "model"));
        } else if (!strcmp(func, "no_models_message")) {
            out = w28_configenv_e9e22f_p2_no_models_message(in);
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
