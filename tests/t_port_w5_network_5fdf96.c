#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_network_5fdf96.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static w5_network_5fdf96_exc_t *build_exc(json_t *j) {
    if (!j || j->type != JSON_OBJECT) return NULL;
    w5_network_5fdf96_exc_t *e = (w5_network_5fdf96_exc_t *)calloc(1, sizeof(*e));
    if (!e) return NULL;
    const char *kind = json_get_str(j, "kind", "");
    e->kind = kind;
    e->code = (int)json_get_num(j, "code", 0);
    json_t *en = json_obj_get(j, "errno_name");
    e->errno_name = (en && en->type == JSON_STRING) ? en->str_val : NULL;
    e->eai_again = json_get_bool(j, "again", false);
    json_t *rsn = json_obj_get(j, "reason");
    e->reason = build_exc(rsn);
    return e;
}
static void free_exc(w5_network_5fdf96_exc_t *e) {
    if (!e) return;
    free_exc(e->reason);
    free(e);
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

        if (!strcmp(func, "transient")) {
            w5_network_5fdf96_exc_t *e = build_exc(json_obj_get(in, "exc"));
            out = json_bool(w5_network_5fdf96_is_transient(e));
            free_exc(e);
        } else if (!strcmp(func, "edge")) {
            json_t *h = json_obj_get(in, "headers");
            size_t nh = json_object_size(h);
            const char **keys = (const char **)calloc(nh ? nh : 1, sizeof(char *));
            const char **vals = (const char **)calloc(nh ? nh : 1, sizeof(char *));
            for (size_t i = 0; i < nh; i++) {
                keys[i] = json_object_get_key_at(h, i);
                json_t *v = json_object_get_at(h, i);
                vals[i] = (v && v->type == JSON_STRING) ? v->str_val : "";
            }
            char *r = w5_network_5fdf96_edge_block((int)json_get_num(in, "code", 0),
                                                   keys, vals, nh,
                                                   json_get_str(in, "filename", ""));
            out = r ? json_string(r) : json_null();
            free(r); free(keys); free(vals);
        } else if (!strcmp(func, "delay")) {
            json_t *ra = json_obj_get(in, "retry_after");
            const char *r = (ra && ra->type == JSON_STRING) ? ra->str_val : NULL;
            out = json_number(w5_network_5fdf96_delay((int)json_get_num(in, "attempt", 0), r));
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
