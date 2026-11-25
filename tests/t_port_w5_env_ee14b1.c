#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_env_ee14b1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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

        if (!strcmp(func, "truthy")) {
            json_t *v = json_obj_get(in, "value");
            w5_env_ee14b1_vkind_t kind = W5_ENV_EE14B1_NULL;
            const char *s = "";
            double num = 0;
            bool b = false;
            if (v && v->type == JSON_STRING) { kind = W5_ENV_EE14B1_STRING; s = v->str_val ? v->str_val : ""; }
            else if (v && v->type == JSON_NUMBER) { kind = W5_ENV_EE14B1_NUMBER; num = v->num_val; }
            else if (v && v->type == JSON_BOOL) { kind = W5_ENV_EE14B1_BOOL; b = v->bool_val; }
            bool def = json_get_bool(in, "default", false);
            out = json_bool(w5_env_ee14b1_is_truthy_value(kind, s, num, b, def));
        } else if (!strcmp(func, "proxy")) {
            char *r = w5_env_ee14b1_normalize_proxy_url(json_get_str(in, "proxy_url", ""));
            out = r ? json_string(r) : json_null();
            free(r);
        } else if (!strcmp(func, "ipv6")) {
            char *r = w5_env_ee14b1_bare_ipv6_literal(json_get_str(in, "entry", ""));
            out = r ? json_string(r) : json_null();
            free(r);
        } else if (!strcmp(func, "sanitize")) {
            char *r = w5_env_ee14b1_sanitize_no_proxy_entries(json_get_str(in, "no_proxy_value", ""));
            out = json_string(r ? r : "");
            free(r);
        } else if (!strcmp(func, "int")) {
            json_t *rawj = json_obj_get(in, "raw");
            const char *raw = (rawj && rawj->type == JSON_STRING) ? rawj->str_val : NULL;
            long long d = (long long)json_get_num(in, "default", 0);
            out = json_number((double)w5_env_ee14b1_env_int_ex(raw, d));
        } else if (!strcmp(func, "float")) {
            json_t *rawj = json_obj_get(in, "raw");
            const char *raw = (rawj && rawj->type == JSON_STRING) ? rawj->str_val : NULL;
            out = json_number(w5_env_ee14b1_env_float_ex(raw, json_get_num(in, "default", 0)));
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
