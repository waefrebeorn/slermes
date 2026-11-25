#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_environments_29900b.h"

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
        char *s = NULL;
        if (!strcmp(func, "replace")) {
            s = w8_environments_29900b_replace(json_get_str(in, "value", ""),
                    json_get_str(in, "old", ""), json_get_str(in, "new", ""),
                    json_get_str(in, "sep", ";"));
        } else if (!strcmp(func, "restart")) {
            s = w8_environments_29900b_restart_needed_ex(json_get_str(in, "running", ""),
                    json_get_str(in, "selected", ""));
        } else if (!strcmp(func, "note")) {
            s = w8_environments_29900b_note_pending(json_obj_get(in, "value"));
        }
        if (s) { json_t *t = json_string(s); if (out) json_free(out); out = t; free(s); }
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
