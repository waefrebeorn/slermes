#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_runturnpr_ae010a.h"
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
        if (!strcmp(func, "repeat_count")) {
            out = w12_runturnpr_ae010a_repeat_count(json_obj_get(in, "message"), json_obj_get(in, "occurrences"));
        } else if (!strcmp(func, "compact")) {
            out = w12_runturnpr_ae010a_compact(json_obj_get(in, "value"), json_obj_get(in, "limit"));
        } else if (!strcmp(func, "progress_text")) {
            out = w12_runturnpr_ae010a_progress_text(json_obj_get(in, "lines"));
        } else if (!strcmp(func, "is_reset_marker")) {
            out = w12_runturnpr_ae010a_is_reset_marker(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "absorb")) {
            out = w12_runturnpr_ae010a_absorb(json_obj_get(in, "progress_lines"), json_obj_get(in, "raw"));
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
        json_free(out); json_free(in);
    }
    return 0;
}
