/* t_port_w8_authcodex_1aa766.c -- JSON-lines harness for src/hermes_cli/auth_codex.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_authcodex_1aa766.h"

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
        if (!strcmp(func, "stripped")) {
            out = w8_authcodex_1aa766_stripped(json_obj_get(in, "value"));
        } else if (!strcmp(func, "is_codex_rate_limit_shaped")) {
            out = w8_authcodex_1aa766_is_codex_rate_limit_shaped(
                json_obj_get(in, "code"), json_obj_get(in, "reason"), json_obj_get(in, "message"));
        } else if (!strcmp(func, "entry_is_rate_limit_exhausted")) {
            out = w8_authcodex_1aa766_entry_is_rate_limit_exhausted(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "codex_usage_probe_url")) {
            out = w8_authcodex_1aa766_codex_usage_probe_url(json_obj_get(in, "base_url"));
        } else if (!strcmp(func, "codex_quota_probe_cache_key")) {
            out = w8_authcodex_1aa766_codex_quota_probe_cache_key(json_get_str(in, "token", ""));
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
