/* JSON-lines harness for the w26_catalogtoo_5874af gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back.
 *
 * The catalog resolver, the skills hub and the approval card are the seams: the
 * "card_surface" and "have_callback" flags stand in for them, so what is
 * compared is the argument verdict and the route decision.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_catalogtoo_5874af.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "check_args")) {
            out = w26_catalogtoo_5874af_check_args(json_obj_get(in, "args"));
        } else if (!strcmp(func, "dispatch")) {
            out = w26_catalogtoo_5874af_dispatch(json_obj_get(in, "args"),
                                                 json_get_bool(in, "card_surface", false),
                                                 json_get_bool(in, "have_callback", false));
        } else if (!strcmp(func, "constants")) {
            out = w26_catalogtoo_5874af_constants();
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
