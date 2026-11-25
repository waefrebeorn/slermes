/* t_port_w13_gitcredent_677b0e.c — Oracle harness for hermes_cli/git_credentials.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_gitcredent_677b0e.h"
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
        if (!strcmp(func, "origin")) {
            char *o = w13_gitcredent_677b0e_https_origin(json_get_str(in, "url", NULL));
            out = o ? json_string(o) : json_null();
            free(o);
        } else if (!strcmp(func, "token_pair")) {
            out = w13_gitcredent_677b0e_token_pair(json_obj_get(in, "token"));
        } else if (!strcmp(func, "auth_headers")) {
            out = w13_gitcredent_677b0e_auth_headers(json_obj_get(in, "env"));
        } else if (!strcmp(func, "credential_required")) {
            out = w13_gitcredent_677b0e_credential_required(json_obj_get(in, "err"), json_obj_get(in, "stdout"));
        } else if (!strcmp(func, "with_git_auth")) {
            out = w13_gitcredent_677b0e_with_git_auth(json_obj_get(in, "env"),
                                                      json_obj_get(in, "url"),
                                                      json_obj_get(in, "auth"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
