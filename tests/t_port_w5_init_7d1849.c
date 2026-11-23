#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_init_7d1849.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        char *p = NULL;
        if (!strcmp(func, "truthy")) {
            out = json_bool(w5_init_7d1849_is_truthy(json_obj_get(in, "value"),
                    json_get_bool(in, "default", false) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "hostname")) {
            p = w5_init_7d1849_base_url_hostname(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "path")) {
            p = w5_init_7d1849_base_url_path(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "origin")) {
            out = w5_init_7d1849_base_url_origin(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "same")) {
            out = json_bool(w5_init_7d1849_same_origin(
                    json_get_str(in, "a", ""), json_get_str(in, "b", "")) ? true : false);
        } else if (!strcmp(func, "matches")) {
            out = json_bool(w5_init_7d1849_host_matches(
                    json_get_str(in, "url", ""), json_get_str(in, "domain", "")) ? true : false);
        } else if (!strcmp(func, "proxy")) {
            p = w5_init_7d1849_normalize_proxy(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "env_int")) {
            out = w5_init_7d1849_env_int(json_get_str(in, "raw", ""),
                                        json_get_str(in, "default", "0"));
        } else if (!strcmp(func, "env_float")) {
            out = w5_init_7d1849_env_float(json_get_str(in, "raw", ""),
                                          json_get_str(in, "default", "0"));
        } else if (!strcmp(func, "noproxy")) {
            p = w5_init_7d1849_sanitize_no_proxy(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "all")) {
            out = w5_init_7d1849_all();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        if (p) { out = json_string(p); free(p); }
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
