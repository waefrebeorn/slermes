/* t_port_w18_credential_ed2097.c — harness for src/agent/credential_auth_recovery.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_credential_ed2097.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "nous_refreshed"))
            out = w18_credential_ed2097_nous_refreshed(json_obj_get(in, "counts"));
        else if (!strcmp(func, "entry_for"))
            out = w18_credential_ed2097_entry_for(json_obj_get(in, "entries"),
                    json_get_str(in, "credential_id", ""), json_get_str(in, "api_key_hint", ""));
        else if (!strcmp(func, "episode"))
            out = w18_credential_ed2097_episode(json_obj_get(in, "counts"),
                    json_get_str(in, "provider", ""), json_obj_get(in, "entry_id"),
                    json_get_num(in, "now", 0));
        else if (!strcmp(func, "constants"))
            out = w18_credential_ed2097_constants();
        else
            out = json_string("UNKNOWN_FUNC");
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
