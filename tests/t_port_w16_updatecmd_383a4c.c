/* JSON-lines harness for the w16_updatecmd_383a4c gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_updatecmd_383a4c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_opt(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "debt")) {
            char *s = w16_updatecmd_383a4c_debt_sha_for_move(
                json_get_str(in, "pre", ""),
                json_get_str(in, "target", ""),
                str_opt(in, "run_start_head"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "table")) {
            out = w16_updatecmd_383a4c_table(
                json_obj_get(in, "obj"),
                json_obj_get(in, "keys"));
        } else if (!strcmp(func, "startup")) {
            out = w16_updatecmd_383a4c_startup_files(
                str_opt(in, "pyproject"),
                json_obj_get(in, "undeclared"));
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
