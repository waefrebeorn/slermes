/* t_port_w5_recovery_15f063.c — harness for pm/recovery.py port.
 * Reads {"func":<name>, ...args...}, emits
 * {"func":<name>,"out":<value>, ...echoed input keys...}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_recovery_15f063.h"

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
        if (!strcmp(func, "w5_recovery_15f063_startup_script")) {
            char *r = w5_recovery_15f063_startup_script();
            out = json_string(r ? r : "");
            free(r);
        } else if (!strcmp(func, "w5_recovery_15f063_normalize_dependency")) {
            char *r = w5_recovery_15f063_normalize_dependency(json_get_str(in, "dep", ""));
            out = json_string(r ? r : "");
            free(r);
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
