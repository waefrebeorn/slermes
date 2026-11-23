/*
 * PoP: w10_installsta_f7e5f6_unit_account  @ hermes_cli/install_state.py:_unit_account
 * PoP: w10_installsta_f7e5f6_onboarding_done @ hermes_cli/install_state.py:_onboarding_done
 * PoP: w10_installsta_f7e5f6_provider_config @ hermes_cli/install_state.py:_provider_config
 * PoP: w10_installsta_f7e5f6_provider_login  @ hermes_cli/install_state.py:_provider_login
 */
#include "hermes_json.h"
#include "noc_w10_installsta_f7e5f6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_space(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}

json_t *w10_installsta_f7e5f6_unit_account(const char *unit) {
    if (!unit) return json_null();
    char *last = NULL;
    size_t last_len = 0;
    const char *p = unit;
    while (1) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        /* One regex line: ^\s*User=\s*(\S+)\s*$ */
        size_t i = 0;
        while (i < len && py_space(p[i])) i++;
        if (len - i >= 5 && !memcmp(p + i, "User=", 5)) {
            i += 5;
            while (i < len && py_space(p[i])) i++;
            size_t s = i;
            while (i < len && !py_space(p[i])) i++;
            size_t cap_len = i - s;
            if (cap_len) {
                size_t j = i;
                while (j < len && py_space(p[j])) j++;
                if (j == len) {
                    free(last);
                    last = (char *)malloc(cap_len + 1);
                    if (last) { memcpy(last, p + s, cap_len); last[cap_len] = 0; last_len = cap_len; }
                }
            }
        }
        (void)last_len;
        if (!e) break;
        p = e + 1;
    }
    if (last) { json_t *r = json_string(last); free(last); return r; }
    return json_null();
}

static json_t *obj_get(json_t *o, const char *k) { return json_obj_get(o, k); }

int w10_installsta_f7e5f6_onboarding_done(json_t *config) {
    json_t *section = obj_get(config, "onboarding");
    if (!section || section->type != JSON_OBJECT) return 0;
    json_t *run = obj_get(section, "run");
    /* section.get("run") is False — identity check, so a JSON 0 does NOT count. */
    return run && run->type == JSON_BOOL && run->bool_val == 0 ? 1 : 0;
}

int w10_installsta_f7e5f6_provider_config(json_t *config) {
    json_t *model = obj_get(config, "model");
    if (!model || model->type != JSON_OBJECT) return 0;
    json_t *prov = obj_get(model, "provider");
    if (!prov || prov->type != JSON_STRING || !prov->str_val) return 0;
    const char *a = prov->str_val, *b = a + strlen(a);
    while (a < b && py_space(*a)) a++;
    while (b > a && py_space(b[-1])) b--;
    size_t n = (size_t)(b - a);
    if (n == 0) return 0;
    if (n == 4 && !memcmp(a, "auto", 4)) return 0;
    return 1;
}

static int py_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0;
        case JSON_STRING: return v->str_val && *v->str_val ? 1 : 0;
        case JSON_ARRAY:  return json_array_size(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

/* str(value) for the "source" value; only the prefix before ':' is compared. */
static const char *py_str_source(json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "None";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) { snprintf(buf, cap, "%.0f", v->num_val); return buf; }
    return "";
}

int w10_installsta_f7e5f6_provider_login(json_t *auth_stores) {
    if (!auth_stores || auth_stores->type != JSON_ARRAY) return 0;
    for (size_t si = 0; si < json_array_size(auth_stores); si++) {
        json_t *store = json_array_get(auth_stores, si);
        if (!store || store->type != JSON_OBJECT) continue;
        json_t *providers = obj_get(store, "providers");
        int hit = 0;
        if (py_truthy(obj_get(store, "active_provider"))) hit = 1;
        if (!hit && providers && providers->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(providers); i++)
                if (py_truthy(json_object_get_at(providers, i))) { hit = 1; break; }
        }
        if (hit) return 1;
        if (py_truthy(obj_get(store, "auth_method")) ||
            py_truthy(obj_get(store, "anon_token")) ||
            py_truthy(obj_get(store, "refresh_token"))) return 1;
        json_t *pool = obj_get(store, "credential_pool");
        if (pool && pool->type == JSON_OBJECT) {
            for (size_t pi = 0; pi < json_object_size(pool); pi++) {
                json_t *rows = json_object_get_at(pool, pi);
                if (!rows || rows->type != JSON_ARRAY) continue;
                for (size_t ri = 0; ri < json_array_size(rows); ri++) {
                    json_t *row = json_array_get(rows, ri);
                    if (!row || row->type != JSON_OBJECT) continue;
                    char buf[64];
                    const char *src = py_str_source(obj_get(row, "source"), buf, sizeof(buf));
                    const char *colon = strchr(src, ':');
                    size_t clen = colon ? (size_t)(colon - src) : strlen(src);
                    if (clen == 6 && !memcmp(src, "manual", 6)) return 1;
                }
            }
        }
    }
    return 0;
}
