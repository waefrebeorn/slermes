#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_configdefa_a05e90.h"

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
        if (!strcmp(func, "env")) {
            out = w18_configdefa_a05e90_env(
                json_get_str(in, "description", ""), json_get_str(in, "prompt", ""),
                json_obj_get(in, "keys"));
        } else if (!strcmp(func, "category")) {
            int pdc = -1;
            json_t *p = json_obj_get(in, "password_default");
            if (p && p->type == JSON_BOOL) pdc = p->bool_val ? 1 : 0;
            int adv = json_get_bool(in, "advanced_default", false) ? 1 : 0;
            out = w18_configdefa_a05e90_category(
                json_get_str(in, "category", ""), pdc, adv,
                json_get_str(in, "description", ""), json_get_str(in, "prompt", ""),
                json_obj_get(in, "opts"));
        } else if (!strcmp(func, "base_url")) {
            out = w18_configdefa_a05e90_base_url(
                json_get_str(in, "name", ""), json_get_str(in, "prompt_name", NULL));
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
