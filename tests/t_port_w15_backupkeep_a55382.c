#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_backupkeep_a55382.h"

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
        if (!strcmp(func, "host_env_key")) {
            out = json_bool(w15_backupkeep_a55382_host_env_key(json_get_str(in, "name", "")));
        } else if (!strcmp(func, "env_name")) {
            out = w15_backupkeep_a55382_env_name(json_get_str(in, "line", ""));
        } else if (!strcmp(func, "host_lines")) {
            out = w15_backupkeep_a55382_host_lines(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "merge_env")) {
            out = w15_backupkeep_a55382_merge_env(json_get_str(in, "archive", ""), json_get_str(in, "host_text", ""));
        } else if (!strcmp(func, "merge_config")) {
            out = w15_backupkeep_a55382_merge_config(json_obj_get(in, "archive"), json_obj_get(in, "host"));
        } else if (!strcmp(func, "merge_auth")) {
            out = w15_backupkeep_a55382_merge_auth(json_obj_get(in, "archive"), json_obj_get(in, "host"));
        } else if (!strcmp(func, "home_parts")) {
            out = w15_backupkeep_a55382_home_parts(json_obj_get(in, "parts"));
        } else if (!strcmp(func, "is_auth_store")) {
            out = json_bool(w15_backupkeep_a55382_is_auth_store(json_obj_get(in, "parts")));
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
