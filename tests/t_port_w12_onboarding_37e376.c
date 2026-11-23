/* t_port_w12_onboarding_37e376.c — JSON-lines oracle harness for hermes_cli/onboarding_run.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_onboarding_37e376.h"

static void emit_key(const char *key, json_t *v)
{
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void)
{
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "explicit_run")) {
            out = w12_onboarding_37e376_explicit_run(json_obj_get(in, "section"));
        } else if (!strcmp(func, "should_run")) {
            bool r = w12_onboarding_37e376_should_run(json_obj_get(in, "explicit"),
                                    json_get_bool(in, "assume_fresh", false),
                                    json_get_bool(in, "fresh", false));
            out = json_bool(r);
        } else if (!strcmp(func, "latch")) {
            bool r = w12_onboarding_37e376_latch(json_obj_get(in, "explicit"),
                               json_get_bool(in, "fresh", false));
            out = json_bool(r);
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
