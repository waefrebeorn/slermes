#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_turnrecove_ba5e00.h"
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
        if (!strcmp(func, "soft_error")) {
            out = w11_turnrecove_ba5e00_codex_soft_failure_error(json_obj_get(in, "error"));
        } else if (!strcmp(func, "token_expired")) {
            int r = w11_turnrecove_ba5e00_is_codex_token_expired(
                json_obj_get(in, "status_code"), json_get_str(in, "reason", ""));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "classify")) {
            out = w11_turnrecove_ba5e00_classify_codex_soft_failure(
                json_get_str(in, "api_mode", ""), json_get_str(in, "status", ""),
                json_obj_get(in, "error"),
                json_get_str(in, "provider", ""), json_get_str(in, "model", ""),
                json_get_str(in, "base_url", ""), json_obj_get(in, "api_key"));
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
