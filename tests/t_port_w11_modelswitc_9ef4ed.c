/* t_port_w11_modelswitc_9ef4ed.c — harness for the model_switch C port. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_modelswitc_9ef4ed.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *sval(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "constants")) {
            out = w11_modelswitc_9ef4ed_constants();
        } else if (!strcmp(func, "lazy_info_route")) {
            out = w11_modelswitc_9ef4ed_lazy_info_route(json_obj_get(in, "override"));
        } else if (!strcmp(func, "current_runtime")) {
            out = w11_modelswitc_9ef4ed_current_runtime(json_obj_get(in, "agent"));
        } else if (!strcmp(func, "result_scope")) {
            out = json_string(w11_modelswitc_9ef4ed_result_scope(
                json_get_bool(in, "one_turn", false),
                json_get_bool(in, "persist_global", false)));
        } else if (!strcmp(func, "apply_result")) {
            out = w11_modelswitc_9ef4ed_apply_result(
                sval(in, "new_model"), sval(in, "warning"),
                json_get_bool(in, "one_turn", false),
                json_get_bool(in, "persist_global", false));
        } else if (!strcmp(func, "confirm_message")) {
            out = w11_modelswitc_9ef4ed_confirm_message(
                sval(in, "warning_message"), sval(in, "result_warning"));
        } else if (!strcmp(func, "switch_away_provider")) {
            out = w11_modelswitc_9ef4ed_switch_away_provider(
                json_get_bool(in, "has_agent", false), sval(in, "explicit_provider"),
                sval(in, "current_provider"), sval(in, "env_provider"),
                json_get_bool(in, "model_seed", false), sval(in, "config_target"));
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
