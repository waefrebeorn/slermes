#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_updatecomp_c7d826.h"

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
        if (!strcmp(func, "exit_status")) {
            out = json_number(w16_updatecomp_c7d826_exit_status(json_get_num(in, "code", 0)));
        } else if (!strcmp(func, "owed")) {
            out = w16_updatecomp_c7d826_owed_user_action(in);
        } else if (!strcmp(func, "write_result")) {
            out = w16_updatecomp_c7d826_write_bootstrap_result(
                in, json_get_num(in, "code", 0), json_obj_get(in, "result_receipt"));
        } else if (!strcmp(func, "report_owed")) {
            out = w16_updatecomp_c7d826_report_desktop_build_owed(
                in, json_obj_get(in, "record"), json_get_str(in, "reason", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
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
