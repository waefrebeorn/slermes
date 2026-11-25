/* t_port_w11_editapprov_59a256.c - harness for acp_adapter/edit_approval.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_editapprov_59a256.h"

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
        const char *body = json_get_str(in, "patch_body", "");
        const char *path = json_get_str(in, "path", "");
        const char *msg = json_get_str(in, "message", "");

        json_t *out = NULL;
        char *text = NULL;
        if (!strcmp(func, "extract")) {
            out = w11_editapprov_59a256_extract_v4a_patch_paths(body);
        } else if (!strcmp(func, "path_is_sensitive")) {
            out = json_bool(w11_editapprov_59a256_path_is_sensitive(path));
        } else if (!strcmp(func, "denied")) {
            text = w11_editapprov_59a256_denied(msg);
            out = json_string(text ? text : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(text);

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
