#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_carriedextr_6bdda2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char **read_arr(json_t *in, const char *key, size_t *n) {
    json_t *a = json_obj_get(in, key);
    if (!a || a->type != JSON_ARRAY) { *n = 0; return NULL; }
    size_t c = json_len(a); *n = c;
    if (!c) return NULL;
    const char **p = (const char **)malloc(c * sizeof(const char *));
    for (size_t i = 0; i < c; i++) {
        json_t *e = json_get(a, i);
        p[i] = (e && e->type == JSON_STRING && e->str_val) ? e->str_val : "";
    }
    return p;
}
static json_t *list_json(const w6_carriedextr_6bdda2_list *l) {
    json_t *a = json_array();
    for (size_t i = 0; i < l->n; i++) json_append(a, json_string(l->v[i] ? l->v[i] : ""));
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
        if (!strcmp(func, "baseline")) {
            size_t ne, nc, na, nb;
            const char **ex = read_arr(in, "extras", &ne);
            const char **ca = read_arr(in, "carried", &nc);
            const char **au = read_arr(in, "automatic", &na);
            const char **ba = read_arr(in, "baseline", &nb);
            int pn = json_get_bool(in, "plugins_none", 0) ? 1 : 0;
            w6_carriedextr_6bdda2_list oe, oc, oa;
            w6_carriedextr_6bdda2_with_pre_pm_baseline(ex, ne, ca, nc, au, na, ba, nb, pn, &oe, &oc, &oa);
            out = json_object();
            json_set(out, "extras", list_json(&oe));
            json_set(out, "carried", list_json(&oc));
            json_set(out, "automatic", list_json(&oa));
            w6_carriedextr_6bdda2_list_free(&oe);
            w6_carriedextr_6bdda2_list_free(&oc);
            w6_carriedextr_6bdda2_list_free(&oa);
            free(ex); free(ca); free(au); free(ba);
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
