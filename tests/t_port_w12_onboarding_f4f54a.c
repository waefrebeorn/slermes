#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_onboarding_f4f54a.h"

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
        if (!strcmp(func, "reset")) {
            w12_onboarding_f4f54a_reset();
            out = json_null();
        } else if (!strcmp(func, "invalidate")) {
            w12_onboarding_f4f54a_invalidate();
            out = json_null();
        } else if (!strcmp(func, "store")) {
            long long gen = strtoll(json_get_str(in, "generation", "0"), NULL, 10);
            int ok = w12_onboarding_f4f54a_store(json_obj_get(in, "value"), gen,
                        json_get_str(in, "before", ""), json_get_str(in, "after", ""));
            out = json_bool(ok ? 1 : 0);
        } else if (!strcmp(func, "read")) {
            out = w12_onboarding_f4f54a_read(json_obj_get(in, "value"),
                    json_get_str(in, "before", ""), json_get_str(in, "after", ""));
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
