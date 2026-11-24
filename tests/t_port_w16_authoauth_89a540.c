/* t_port_w16_authoauth_89a540.c — harness for src/hermes_cli/auth_oauth_grants.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_authoauth_89a540.h"

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
        if (!strcmp(func, "is_oauth_pool_payload"))
            out = json_bool(w16_authoauth_89a540_is_oauth_pool_payload(json_obj_get(in, "entry")));
        else if (!strcmp(func, "is_forkable_pool_row"))
            out = json_bool(w16_authoauth_89a540_is_forkable_pool_row(
                json_get_str(in, "provider_id", NULL), json_obj_get(in, "entry")));
        else if (!strcmp(func, "block_tokens"))
            out = w16_authoauth_89a540_block_tokens(json_obj_get(in, "block"));
        else if (!strcmp(func, "is_pkce_row"))
            out = json_bool(w16_authoauth_89a540_is_pkce_row(json_obj_get(in, "row")));
        else if (!strcmp(func, "sign"))
            out = w16_authoauth_89a540_sign(json_obj_get(in, "x"), json_obj_get(in, "y"));
        else if (!strcmp(func, "constants"))
            out = w16_authoauth_89a540_constants();
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
