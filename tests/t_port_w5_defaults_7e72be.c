/* t_port_w5_defaults_7e72be.c — differential harness for pm/defaults.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_defaults_7e72be.h"

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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "canonical")) {
            char buf[512];
            w5defaults_canonical(json_get_str(in, "name", ""), buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "declined")) {
            char **res = NULL;
            size_t rn = 0;
            w5defaults_declined_from_json(json_get_str(in, "text", ""), &res, &rn);
            out = strlist(res, rn);
            w5defaults_free_list(res, rn);
        } else if (!strcmp(func, "defaults")) {
            char **names = NULL;
            size_t nn = arr_strings(json_obj_get(in, "names"), &names);
            json_t *pj = json_obj_get(in, "pkgs");
            size_t np = json_is_array(pj) ? json_len(pj) : 0;
            w5defaults_pkg_t *pkgs = np ? (w5defaults_pkg_t *)calloc(np, sizeof(*pkgs)) : NULL;
            for (size_t i = 0; i < np; i++) {
                json_t *e = json_get(pj, i);
                pkgs[i].name = json_get_str(e, "name", "");
                pkgs[i].is_default = json_get_bool(e, "default", 0) ? 1 : 0;
                pkgs[i].is_internal = json_get_bool(e, "internal", 0) ? 1 : 0;
                pkgs[i].missing = json_get_bool(e, "missing", 0) ? 1 : 0;
                pkgs[i].found = json_get_bool(e, "found", 1) ? 1 : 0;
            }
            char **dec = NULL;
            size_t nd = arr_strings(json_obj_get(in, "declined"), &dec);
            char **res = NULL;
            size_t rn = 0;
            w5defaults_default_packages((const char **)names, nn, pkgs, np,
                                        (const char **)dec, nd, &res, &rn);
            out = strlist(res, rn);
            w5defaults_free_list(res, rn);
            free(dec);          /* borrowed from the parsed JSON tree */
            free(pkgs);
            free(names);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
