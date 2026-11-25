/* JSON-lines harness for the w28_configenv_e9e22f_p1 gate (config_env env surface).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_configenv_e9e22f_p1.h"

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
        if (!strcmp(func, "env_preview")) {
            out = w28_configenv_e9e22f_p1_env_preview(json_obj_get(in, "value"));
        } else if (!strcmp(func, "is_redacted_preview")) {
            out = w28_configenv_e9e22f_p1_is_redacted_preview(json_obj_get(in, "submitted"));
        } else if (!strcmp(func, "env_vars")) {
            out = w28_configenv_e9e22f_p1_env_vars(json_obj_get(in, "optional_env_vars"),
                                                   json_obj_get(in, "catalog_meta"),
                                                   json_obj_get(in, "env_on_disk"),
                                                   json_obj_get(in, "channel_keys"));
        } else if (!strcmp(func, "env_error")) {
            out = w28_configenv_e9e22f_p1_env_error(json_obj_get(in, "kind"),
                                                    json_obj_get(in, "message"),
                                                    json_obj_get(in, "status"),
                                                    json_obj_get(in, "detail"));
        } else if (!strcmp(func, "custom_endpoint_id")) {
            out = w28_configenv_e9e22f_p1_custom_endpoint_id(json_obj_get(in, "raw"),
                                                             json_get_str(in, "fallback", "custom"));
        } else if (!strcmp(func, "models_from_entry")) {
            out = w28_configenv_e9e22f_p1_models_from_entry(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "api_key_display")) {
            out = w28_configenv_e9e22f_p1_api_key_display(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "endpoint_api_mode")) {
            out = w28_configenv_e9e22f_p1_endpoint_api_mode(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "model_names_provider")) {
            out = w28_configenv_e9e22f_p1_model_names_provider(json_obj_get(in, "model_cfg"),
                                                               json_obj_get(in, "provider_key"),
                                                               json_obj_get(in, "entry"));
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
