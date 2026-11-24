/* JSON-lines harness for the w21_runtimepro_eb8ed1 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_runtimepro_eb8ed1.h"
#include "hermes_cli/runtime_provider_missing_key.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "declared_key_env")) {
            char *s = w21_runtimepro_eb8ed1_declared_key_env(json_obj_get(in, "entry"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "declared_custom_key")) {
            out = w21_runtimepro_eb8ed1_declared_custom_key(json_get_str(in, "requested_provider", ""),
                                                           json_obj_get(in, "runtime"),
                                                           json_obj_get(in, "model_cfg"),
                                                           json_obj_get(in, "entries"));
        } else if (!strcmp(func, "where_to_set_key")) {
            json_t *routed = json_obj_get(in, "serves_routed");
            json_t *has = json_obj_get(in, "env_has_var");
            char *s = w21_runtimepro_eb8ed1_where_to_set_key(
                json_get_str(in, "env_var", ""), json_get_str(in, "profile", "default"),
                json_get_str(in, "display_home", ""), json_get_str(in, "cli_selector", ""),
                routed && routed->type == JSON_BOOL ? routed->bool_val : 0,
                has && has->type == JSON_BOOL ? has->bool_val : 0);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "named_profile_key_hint")) {
            json_t *routed = json_obj_get(in, "serves_routed");
            json_t *has = json_obj_get(in, "env_has_var");
            char *s = w21_runtimepro_eb8ed1_named_profile_key_hint(
                json_obj_get(in, "env_vars"), json_get_str(in, "profile", NULL),
                json_get_str(in, "display_home", ""), json_get_str(in, "cli_selector", ""),
                routed && routed->type == JSON_BOOL ? routed->bool_val : 0,
                has && has->type == JSON_BOOL ? has->bool_val : 0);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "raise_if_missing")) {
            out = w21_runtimepro_eb8ed1_raise_if_declared_key_missing(
                json_get_str(in, "requested_provider", NULL), json_obj_get(in, "runtime"),
                json_obj_get(in, "model_cfg"), json_obj_get(in, "entries"),
                json_obj_get(in, "secrets"), json_obj_get(in, "explicit_api_key"),
                json_obj_get(in, "env"));
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
