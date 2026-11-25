#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_webappacce_9bd4bc.h"
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
        if (!strcmp(func, "sign_in_client")) {
            out = w11_webappacce_9bd4bc_sign_in_client(json_obj_get(in, "record"));
        } else if (!strcmp(func, "mode")) {
            const char *m = w11_webappacce_9bd4bc_mode(json_obj_get(in, "settings"),
                        json_get_bool(in, "wildcard", false) ? 1 : 0);
            out = m ? json_string(m) : NULL;
        } else if (!strcmp(func, "serves_config")) {
            out = json_bool(w11_webappacce_9bd4bc_serves_config(json_obj_get(in, "status")) ? true : false);
        } else if (!strcmp(func, "write_config")) {
            out = w11_webappacce_9bd4bc_write_config(json_obj_get(in, "cfg"),
                        json_get_bool(in, "enabled", false) ? 1 : 0,
                        json_get_str(in, "host", ""), json_get_str(in, "public_url", ""),
                        json_obj_get(in, "port"));
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
