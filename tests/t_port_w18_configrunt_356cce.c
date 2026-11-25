#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_configrunt_356cce.h"

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
        if (!strcmp(func, "covered")) {
            out = json_bool(w18_configrunt_356cce_covered(
                json_get_str(in, "key", ""), json_obj_get(in, "paths")) ? 1 : 0);
        } else if (!strcmp(func, "is_runtime_read_key")) {
            const json_t *p = json_obj_get(in, "preset");
            int preset = (p && p->type == JSON_BOOL && p->bool_val) ? 1 : 0;
            out = json_bool(w18_configrunt_356cce_is_runtime_read_key(
                json_obj_get(in, "parts"), preset,
                json_obj_get(in, "selection"), json_obj_get(in, "plugin")) ? 1 : 0);
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
