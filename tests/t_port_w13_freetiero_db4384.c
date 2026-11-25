#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_freetiero_db4384.h"

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
        if (!strcmp(func, "constants")) {
            out = w13_freetiero_db4384_constants();
        } else if (!strcmp(func, "due_at")) {
            out = w13_freetiero_db4384_due_at(json_obj_get(in, "state"));
        } else if (!strcmp(func, "due_in")) {
            out = w13_freetiero_db4384_due_in(json_obj_get(in, "state"),
                        json_get_bool(in, "eligible", false) ? 1 : 0,
                        json_get_num(in, "now", 0));
        } else if (!strcmp(func, "eligible")) {
            out = w13_freetiero_db4384_eligible(
                        json_get_str(in, "kind", "trial"),
                        json_obj_get(in, "flag"),
                        json_get_bool(in, "is_guest", false) ? 1 : 0,
                        json_get_bool(in, "guest_allowed", false) ? 1 : 0,
                        json_get_bool(in, "free_active", false) ? 1 : 0);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
