/* JSON-lines harness for the w26_telegramla_b660b0 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramla_b660b0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *err_named(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize")) {
            const char *prose = json_get_str(in, "prose", NULL);
            out = prose ? w26_telegramla_b660b0_normalize(prose) : err_named("TypeError");
        } else if (!strcmp(func, "replace_outside")) {
            const char *prose = json_get_str(in, "prose", NULL);
            json_t *spans = json_obj_get(in, "kept_spans");
            if (!prose) out = err_named("TypeError");
            else if (!spans || spans->type != JSON_ARRAY) out = err_named("TypeError");
            else out = w26_telegramla_b660b0_replace_outside(prose, spans);
        } else if (!strcmp(func, "constants")) {
            out = w26_telegramla_b660b0_constants();
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
