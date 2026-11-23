/* t_port_w14_auxiliaryt_39927b.c — JSON-lines harness for auxiliary_task_config.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_auxiliaryt_39927b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_arg(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "authoritative_auxiliary_task")) {
            char *r = w14_auxiliaryt_39927b_authoritative_auxiliary_task(str_arg(in, "task"));
            out = json_string(r ? r : "");
            free(r);
        } else if (!strcmp(func, "core_rooted")) {
            out = json_bool(w14_auxiliaryt_39927b_core_rooted(str_arg(in, "base"), json_obj_get(in, "entries")));
        } else if (!strcmp(func, "layer_over_inherited")) {
            out = w14_auxiliaryt_39927b_layer_over_inherited(json_obj_get(in, "inherited"), json_obj_get(in, "user"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
