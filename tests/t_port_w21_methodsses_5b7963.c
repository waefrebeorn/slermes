/* JSON-lines harness for the w21_methodsses_5b7963 gate (session model guard).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_methodsses_5b7963.h"

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
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "str")) {
            char *s = w21_methodsses_5b7963_str_or_empty(json_obj_get(in, "value"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "truthy")) {
            out = json_bool(w21_methodsses_5b7963_truthy(json_obj_get(in, "value")));
        } else if (!strcmp(func, "reasoning")) {
            out = w21_methodsses_5b7963_reasoning(json_obj_get(in, "effort"));
        } else if (!strcmp(func, "service_tier")) {
            out = w21_methodsses_5b7963_service_tier(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "model_override")) {
            out = w21_methodsses_5b7963_model_override(json_obj_get(in, "params"),
                                                       str_or(in, "scope_provider", ""),
                                                       json_obj_get(in, "conflict"));
        } else if (!strcmp(func, "create_overrides")) {
            out = w21_methodsses_5b7963_create_overrides(json_obj_get(in, "params"));
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
