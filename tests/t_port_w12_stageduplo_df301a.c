/* t_port_w12_stageduplo_df301a.c — JSON-lines harness for hermes_cli/staged_uploads.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_stageduplo_df301a.h"

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
        char buf[4096];
        if (!strcmp(func, "safe_filename")) {
            w12_stageduplo_df301a_safe_filename(json_get_str(in, "value", NULL),
                                                buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "parse_staged_upload")) {
            out = w12_stageduplo_df301a_parse_staged_upload(
                json_get_str(in, "install_id", NULL), json_obj_get(in, "descriptor"));
        } else if (!strcmp(func, "staged_upload_descriptor")) {
            out = w12_stageduplo_df301a_staged_upload_descriptor(
                json_get_str(in, "install_id", NULL),
                json_get_str(in, "path", ""),
                json_get_str(in, "profile_home", ""));
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
