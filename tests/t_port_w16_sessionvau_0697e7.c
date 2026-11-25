/* t_port_w16_sessionvau_0697e7.c — JSON-lines harness for gateway/session_vault_prompt.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionvau_0697e7.h"

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
        if (!strcmp(func, "unlock_params"))
            out = w16_sessionvau_0697e7_unlock_params(json_obj_get(in, "backend"), json_obj_get(in, "display_name"));
        else if (!strcmp(func, "save_login_params"))
            out = w16_sessionvau_0697e7_save_login_params(json_obj_get(in, "origin"), json_obj_get(in, "site"));
        else if (!strcmp(func, "code_params"))
            out = w16_sessionvau_0697e7_code_params(json_obj_get(in, "site"), json_obj_get(in, "hint"));
        else if (!strcmp(func, "save_login"))
            out = w16_sessionvau_0697e7_save_login(json_get_str(in, "raw", NULL));
        else if (!strcmp(func, "timeouts"))
            out = w16_sessionvau_0697e7_timeouts();
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
