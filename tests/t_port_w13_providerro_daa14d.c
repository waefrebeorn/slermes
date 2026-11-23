#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_providerro_daa14d.h"

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
        if (!strcmp(func, "freeze")) {
            out = w13_providerro_daa14d_freeze(json_obj_get(in, "value"));
        } else if (!strcmp(func, "forces_max")) {
            out = json_bool(w13_providerro_daa14d_forces_max(json_get_str(in, "model", "")) ? true : false);
        } else if (!strcmp(func, "route")) {
            out = w13_providerro_daa14d_route(json_get_str(in, "hook", ""),
                     json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""),
                     json_obj_get(in, "profiles"), json_obj_get(in, "providers"),
                     json_obj_get(in, "hosts"));
        } else if (!strcmp(func, "ask")) {
            out = w13_providerro_daa14d_ask(json_obj_get(in, "profile"),
                     json_get_str(in, "hook", ""), json_obj_get(in, "fallback"),
                     json_obj_get(in, "scope"), json_obj_get(in, "answers"),
                     json_obj_get(in, "raises"));
        } else if (!strcmp(func, "hook_failed")) {
            out = json_bool(w13_providerro_daa14d_hook_failed(json_obj_get(in, "profile"),
                     json_get_str(in, "hook", ""), json_obj_get(in, "scope")) ? true : false);
        } else if (!strcmp(func, "model_api_mode")) {
            out = w13_providerro_daa14d_model_api_mode(json_obj_get(in, "profile"),
                     json_get_str(in, "model", ""));
        } else if (!strcmp(func, "route_answer")) {
            out = w13_providerro_daa14d_route_answer(json_get_str(in, "hook", ""),
                     json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""),
                     json_obj_get(in, "profiles"), json_obj_get(in, "providers"),
                     json_obj_get(in, "hosts"), json_obj_get(in, "scope"));
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
