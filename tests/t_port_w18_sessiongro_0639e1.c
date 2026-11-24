/* JSON-lines harness for the w18_sessiongro_0639e1 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_sessiongro_0639e1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "capability_for_method")) {
            out = json_string(w18_sessiongro_0639e1_capability_for_method(str_or(in, "method", "")));
        } else if (!strcmp(func, "fields_for_method")) {
            out = w18_sessiongro_0639e1_fields_for_method(str_or(in, "method", ""));
        } else if (!strcmp(func, "params_allowed")) {
            out = json_bool(w18_sessiongro_0639e1_params_allowed(str_or(in, "method", ""),
                                                                 json_obj_get(in, "params")) ? 1 : 0);
        } else if (!strcmp(func, "refusal")) {
            out = w18_sessiongro_0639e1_refusal(str_or(in, "reason", ""),
                                                json_obj_get(in, "admitted"));
        } else if (!strcmp(func, "ui_meta_revisions")) {
            out = w18_sessiongro_0639e1_ui_meta_revisions(json_obj_get(in, "meta"));
        } else if (!strcmp(func, "stat_key")) {
            out = w18_sessiongro_0639e1_stat_key(str_or(in, "mtime_ns", "0"),
                                                 str_or(in, "ctime_ns", "0"),
                                                 str_or(in, "size", "0"),
                                                 str_or(in, "ino", "0"));
        } else if (!strcmp(func, "has_avatar")) {
            out = json_bool(w18_sessiongro_0639e1_has_avatar(str_or(in, "home", "")) ? 1 : 0);
        } else if (!strcmp(func, "execution_control")) {
            out = w18_sessiongro_0639e1_execution_control(str_or(in, "method", ""),
                                                          json_obj_get(in, "params"),
                                                          json_obj_get(in, "service"));
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
