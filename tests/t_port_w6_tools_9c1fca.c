#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_tools_9c1fca.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long num_of(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : def;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "tool_kind")) {
            const char *s = w6_tools_9c1fca_tool_kind(json_get_str(in, "tool_name", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "plural")) {
            out = w6_tools_9c1fca_plural(json_obj_get(in, "count"),
                                         json_get_str(in, "word", ""),
                                         json_get_str(in, "suffix", "s"));
        } else if (!strcmp(func, "nonempty")) {
            out = w6_tools_9c1fca_nonempty(json_obj_get(in, "result"));
        } else if (!strcmp(func, "failure")) {
            out = w6_tools_9c1fca_failure(json_obj_get(in, "data"), json_get_str(in, "prefix", ""));
        } else if (!strcmp(func, "truncate_text")) {
            out = w6_tools_9c1fca_truncate_text(json_get_str(in, "text", ""),
                                                num_of(in, "limit", 5000));
        } else if (!strcmp(func, "fenced_text")) {
            out = w6_tools_9c1fca_fenced_text(json_get_str(in, "text", ""),
                                              json_get_str(in, "language", ""));
        } else if (!strcmp(func, "json_loads_maybe")) {
            out = w6_tools_9c1fca_json_loads_maybe(json_obj_get(in, "value"));
        } else if (!strcmp(func, "coerce_args")) {
            out = w6_tools_9c1fca_coerce_args(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "more")) {
            out = w6_tools_9c1fca_more(json_obj_get(in, "items"),
                                       num_of(in, "shown", 0), json_get_str(in, "unit", ""));
        } else if (!strcmp(func, "arg")) {
            out = w6_tools_9c1fca_arg(json_obj_get(in, "args"), json_obj_get(in, "keys"),
                                      json_get_str(in, "default", ""));
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
