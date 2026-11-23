/* t_port_w5_testenv_ca5da8.c — differential harness for pm/testenv.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_testenv_ca5da8.h"

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
static json_t *constlist(const char *const *items, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
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

        if (!strcmp(func, "constants")) {
            size_t ne = 0, ng = 0, nd = 0;
            const char *const *ex = w5testenv_default_extras(&ne);
            const char *const *gr = w5testenv_groups(&ng);
            const char *const *dv = w5testenv_developer_const(&nd);
            json_t *o = json_object();
            json_set(o, "name", json_string(w5testenv_name()));
            json_set(o, "default_extras", constlist(ex, ne));
            json_set(o, "groups", constlist(gr, ng));
            json_set(o, "developer_tools", constlist(dv, nd));
            out = o;
        } else if (!strcmp(func, "devtools")) {
            json_t *p = json_obj_get(in, "pinned");
            size_t n = json_is_array(p) ? json_len(p) : 0;
            const char **items = n ? (const char **)calloc(n, sizeof(char *)) : NULL;
            if (items) {
                for (size_t i = 0; i < n; i++) {
                    json_t *e = json_get(p, i);
                    items[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
                }
            }
            char **res = NULL;
            size_t rn = 0;
            w5testenv_developer_tools(items, n, &res, &rn);
            out = strlist(res, rn);
            w5testenv_free_list(res, rn);
            free(items);
        } else if (!strcmp(func, "parse")) {
            char **res = NULL;
            size_t rn = 0;
            w5testenv_parse_extras(json_get_str(in, "value", ""), &res, &rn);
            out = rn ? strlist(res, rn) : json_null();
            w5testenv_free_list(res, rn);
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
