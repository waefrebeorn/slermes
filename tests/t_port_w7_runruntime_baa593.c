#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_runruntime_baa593.h"

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
        if (!strcmp(func, "profile_parked")) {
            out = json_bool(w7_runruntime_baa593_profile_parked(
                json_obj_get(in, "descriptor"), json_get_str(in, "name", "")) != 0);
        } else if (!strcmp(func, "parked_profile_map")) {
            out = w7_runruntime_baa593_parked_profile_map(json_obj_get(in, "descriptor"));
        } else if (!strcmp(func, "park_profile")) {
            out = w7_runruntime_baa593_park_profile(json_obj_get(in, "descriptor"),
                json_get_str(in, "name", ""), json_obj_get(in, "reason"));
        } else if (!strcmp(func, "unpark_profile")) {
            out = w7_runruntime_baa593_unpark_profile(json_obj_get(in, "descriptor"),
                json_get_str(in, "name", ""));
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
