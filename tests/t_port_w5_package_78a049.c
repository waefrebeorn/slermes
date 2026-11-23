#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_package_78a049.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        char *tmp = NULL;

        if (!strcmp(func, "compose")) {
            json_t *diffs = json_obj_get(in, "diffs");
            size_t nd = json_is_array(diffs) ? json_len(diffs) : 0;
            w5_package_78a049_diff_item_t items[512];
            w5_package_78a049_diff_t darr[64];
            size_t nitems = 0, ndarr = 0;
            if (nd > 64) nd = 64;
            for (size_t i = 0; i < nd; i++) {
                json_t *d = json_get(diffs, i);
                darr[ndarr].items = &items[nitems];
                darr[ndarr].n_items = 0;
                size_t nk = json_object_size(d);
                for (size_t k = 0; k < nk && nitems < 512; k++) {
                    const char *key = json_object_get_key_at(d, k);
                    json_t *val = json_object_get_at(d, k);
                    w5_package_78a049_diff_item_t *x = &items[nitems++];
                    x->key = key ? key : "";
                    x->is_list = val && val->type == JSON_ARRAY;
                    x->value = (val && val->type == JSON_STRING) ? val->str_val : "";
                    x->items = NULL; x->n_items = 0;
                    if (x->is_list) {
                        x->n_items = json_len(val);
                        const char **ptr = (const char **)calloc(x->n_items ? x->n_items : 1, sizeof(char *));
                        x->items = ptr;
                        for (size_t q = 0; q < x->n_items; q++) {
                            json_t *e = json_get(val, q);
                            ptr[q] = (e && e->type == JSON_STRING) ? e->str_val : "";
                        }
                    }
                    darr[ndarr].n_items++;
                }
                ndarr++;
            }
            json_t *base = json_obj_get(in, "base");
            size_t nb = json_object_size(base);
            w5_package_78a049_kv_t kv[512];
            if (nb > 512) nb = 512;
            for (size_t i = 0; i < nb; i++) {
                kv[i].key = json_object_get_key_at(base, i);
                json_t *v = json_object_get_at(base, i);
                kv[i].value = (v && v->type == JSON_STRING) ? v->str_val : "";
            }
            out = w5_package_78a049_compose_env(darr, ndarr, kv, nb);
            for (size_t i = 0; i < nitems; i++) free((void *)items[i].items);
        } else if (!strcmp(func, "error")) {
            tmp = w5_package_78a049_install_error(
                jstr(in, "package") ? jstr(in, "package") : "",
                jstr(in, "cause") ? jstr(in, "cause") : "", jstr(in, "remedy"));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "entry")) {
            tmp = w5_package_78a049_store_entry(jstr(in, "name") ? jstr(in, "name") : "",
                jstr(in, "version") ? jstr(in, "version") : "",
                jstr(in, "target") ? jstr(in, "target") : "");
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "url")) {
            tmp = w5_package_78a049_fetch_url(jstr(in, "url") ? jstr(in, "url") : "",
                jstr(in, "version") ? jstr(in, "version") : "",
                jstr(in, "target") ? jstr(in, "target") : "");
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "optional")) {
            json_t *od = json_obj_get(in, "on_demand");
            size_t n = json_is_array(od) ? json_len(od) : 0;
            const char **arr = (const char **)calloc(n ? n : 1, sizeof(char *));
            for (size_t i = 0; i < n; i++) {
                json_t *e = json_get(od, i);
                arr[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
            }
            out = json_bool(w5_package_78a049_is_optional(json_get_bool(in, "optional", false),
                                                           arr, n, json_get_str(in, "target", "")));
            free(arr);
        } else if (!strcmp(func, "reason")) {
            json_t *gaps = json_obj_get(in, "gaps");
            size_t n = json_is_array(gaps) ? json_len(gaps) : 0;
            const char **tg = (const char **)calloc(n ? n : 1, sizeof(char *));
            const char **rs = (const char **)calloc(n ? n : 1, sizeof(char *));
            for (size_t i = 0; i < n; i++) {
                json_t *pair = json_get(gaps, i);
                tg[i] = jstr(pair, "target");
                rs[i] = jstr(pair, "reason");
            }
            tmp = w5_package_78a049_missing_reason(tg, rs, n, json_get_str(in, "target", ""));
            out = tmp ? json_string(tmp) : json_null();
            free(tg); free(rs);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(tmp);

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
