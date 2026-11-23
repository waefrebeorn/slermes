/* t_port_w5_features_c0a63b.c — differential harness for pm/features.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_features_c0a63b.h"

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

        if (!strcmp(func, "payload")) {
            char **ex = NULL;
            size_t ne = arr_strings(json_obj_get(in, "extras"), &ex);
            char *s = w5features_write_payload((const char **)ex, ne);
            out = json_string(s ? s : "");
            free(s);
            free(ex);
        } else if (!strcmp(func, "read")) {
            char **res = NULL;
            size_t rn = 0;
            int ok = w5features_read_payload(json_get_str(in, "text", ""), &res, &rn);
            out = ok ? strlist(res, rn) : json_null();
            w5features_free_list(res, rn);
        } else if (!strcmp(func, "optin")) {
            char **nm = NULL, **dc = NULL;
            size_t nn = arr_strings(json_obj_get(in, "names"), &nm);
            size_t nd = arr_strings(json_obj_get(in, "declared"), &dc);
            char **res = NULL;
            size_t rn = 0;
            int ok = w5features_opt_in((const char **)nm, nn, (const char **)dc, nd, &res, &rn);
            out = ok ? strlist(res, rn) : json_null();
            w5features_free_list(res, rn);
            free(nm);
            free(dc);
        } else if (!strcmp(func, "path")) {
            char b[2048];
            w5features_path(json_get_str(in, "base", ""), b, sizeof(b));
            out = json_string(b);
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
