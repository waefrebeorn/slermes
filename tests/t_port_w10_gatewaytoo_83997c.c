#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_gatewaytoo_83997c.h"
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
        if (!strcmp(func, "proc_from_row")) {
            out = w10_gatewaytoo_83997c_proc_from_row(json_obj_get(in, "row"));
        } else if (!strcmp(func, "filter_printable")) {
            out = w10_gatewaytoo_83997c_filter_printable(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "interesting")) {
            out = json_bool(w10_gatewaytoo_83997c_interesting(json_obj_get(in, "rows"),
                            json_get_num(in, "now", 0.0)) ? true : false);
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
