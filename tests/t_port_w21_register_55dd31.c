/* JSON-lines harness for the w21_register_55dd31 gate (qqbot register).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_register_55dd31.h"
#include "plugins/platforms/qqbot/register.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A fixture string, or NULL when absent / not a string. */
static const char *opt_str(json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_connected")) {
            json_t *env = json_obj_get(in, "env");
            out = json_bool(w21_register_55dd31_is_connected(json_obj_get(in, "extra"),
                                                             opt_str(env, "QQ_APP_ID"),
                                                             opt_str(env, "QQ_CLIENT_SECRET")));
        } else if (!strcmp(func, "config_connected")) {
            out = json_bool(w21_register_55dd31_config_connected(json_obj_get(in, "extra"),
                                                                 json_obj_get(in, "token"),
                                                                 json_obj_get(in, "api_key")));
        } else if (!strcmp(func, "hint")) {
            out = json_string(w21_register_55dd31_platform_hint());
        } else if (!strcmp(func, "meta")) {
            out = w21_register_55dd31_register_meta();
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
