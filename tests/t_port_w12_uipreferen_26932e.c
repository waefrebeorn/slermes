/* t_port_w12_uipreferen_26932e.c — JSON-lines harness for hermes_cli/ui_preferences.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_uipreferen_26932e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    /* MAX_VALUE_CHARS boundary case is ~256 KiB: the line buffer must hold it. */
    static char line[400000];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char buf[2048];
        if (!strcmp(func, "sanitize")) {
            out = w12_uipreferen_26932e_sanitize(json_obj_get(in, "values"));
        } else if (!strcmp(func, "merge")) {
            out = w12_uipreferen_26932e_merge(json_obj_get(in, "current"),
                                              json_obj_get(in, "changes"),
                                              json_get_bool(in, "seed", false) ? 1 : 0);
        } else if (!strcmp(func, "validate")) {
            int ok = w12_uipreferen_26932e_validate(json_obj_get(in, "changes"),
                                                    buf, sizeof(buf));
            out = json_object();
            json_set(out, "ok", json_bool(ok ? true : false));
            json_set(out, "error", ok ? json_null() : json_string(buf));
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
