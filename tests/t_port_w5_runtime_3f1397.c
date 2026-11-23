/* t_port_w5_runtime_3f1397.c — differential harness for pm/runtime.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_runtime_3f1397.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *strlist(char **items, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i] ? items[i] : ""));
    return a;
}
static size_t arr_strings(json_t *arr, char ***out) {
    size_t n = json_is_array(arr) ? json_len(arr) : 0;
    char **items = n ? (char **)calloc(n, sizeof(char *)) : NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        items[i] = (char *)((e && e->type == JSON_STRING) ? e->str_val : "");
    }
    *out = items;
    return n;
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

        if (!strcmp(func, "validate")) {
            int rc = (int)json_get_num(in, "rc", 0);
            char *s = w5runtime_validate_result(rc, json_get_str(in, "stderr", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "command")) {
            char **args = NULL;
            size_t na = arr_strings(json_obj_get(in, "args"), &args);
            char **res = NULL;
            size_t rn = 0;
            w5runtime_command_args(json_get_bool(in, "resident", 0) ? 1 : 0,
                                   json_get_str(in, "python", ""),
                                   json_get_str(in, "site", ""),
                                   json_get_str(in, "script", ""),
                                   (const char **)args, na, &res, &rn);
            out = strlist(res, rn);
            w5runtime_free_list(res, rn);
            free(args);
        } else if (!strcmp(func, "launcher")) {
            out = json_string(w5runtime_launcher());
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
