#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_indexconfig_5a61c5.h"

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
        char *s = NULL;
        if (!strcmp(func, "is_forwarded")) {
            out = json_bool(w5_indexconfig_5a61c5_is_forwarded(json_get_str(in, "key", "")) != 0);
        } else if (!strcmp(func, "is_index_redirect")) {
            out = json_bool(w5_indexconfig_5a61c5_is_index_redirect(json_get_str(in, "key", "")) != 0);
        } else if (!strcmp(func, "ini_unsafe")) {
            s = w5_indexconfig_5a61c5_ini_unsafe(json_get_str(in, "raw", ""));
        } else if (!strcmp(func, "npmrc_env_replace")) {
            s = w5_indexconfig_5a61c5_npmrc_env_replace(json_get_str(in, "text", ""),
                                                        json_obj_get(in, "env"));
        } else if (!strcmp(func, "npm_registry")) {
            s = w5_indexconfig_5a61c5_npm_registry_ex(json_get_str(in, "env_registry", ""),
                                                      json_get_str(in, "npmrc_registry", ""));
        } else if (!strcmp(func, "npm_registry_url")) {
            s = w5_indexconfig_5a61c5_npm_registry_url_ex(json_get_str(in, "url", ""),
                                    json_get_str(in, "env_registry", ""),
                                    json_get_str(in, "npmrc_registry", ""));
        } else if (!strcmp(func, "bridged_index_settings")) {
            out = w5_indexconfig_5a61c5_bridged_index_settings_ex(
                json_obj_get(in, "ambient"), json_get_str(in, "pip_conf", ""));
        }
        if (s) { free(out); out = json_string(s); free(s); }
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
