#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_mcptoolerr_c679c5.h"

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
        if (!strcmp(func, "resolve_identity_header")) {
            out = w8_mcptoolerr_c679c5_resolve_identity_header(
                json_get_str(in, "name", ""), json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "apply_identity_header")) {
            out = w8_mcptoolerr_c679c5_apply_identity_header(
                json_get_str(in, "name", ""), json_obj_get(in, "cfg"), json_obj_get(in, "headers"));
        } else if (!strcmp(func, "validate_remote_mcp_url")) {
            out = w8_mcptoolerr_c679c5_validate_remote_mcp_url(
                json_get_str(in, "name", ""), json_obj_get(in, "url"));
        } else if (!strcmp(func, "shown_url")) {
            char *s = w8_mcptoolerr_c679c5_shown_url(json_get_str(in, "url", ""));
            out = json_string(s ? s : "");
            free(s);
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
