#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_updatewind_e677b5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int elevated_arg(const json_t *in) {
    json_t *e = json_obj_get(in, "elevated");
    if (!e || e->type == JSON_NULL) return -1;   /* Python None */
    if (e->type == JSON_BOOL) return e->bool_val ? 1 : 0;
    return -1;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *home = json_get_str(in, "home", "");
        bool probe = json_get_bool(in, "probe_elevated", false);
        json_t *out = NULL;

        if (!strcmp(func, "repair")) {
            char *msg = w17_updatewind_e677b5_repair(
                elevated_arg(in), probe, home,
                json_obj_get(in, "granted"), json_obj_get(in, "skipped"), json_obj_get(in, "failed"));
            out = msg ? json_string(msg) : json_null();
            free(msg);
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
