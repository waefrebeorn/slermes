#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_codexservi_5c6a93.h"

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
        json_t *entry = json_obj_get(in,"entry");
        if (!strcmp(func,"advertised_tiers"))
            out = w11_codexservi_5c6a93_advertised_tiers(entry);
        else if (!strcmp(func,"advertised_cyber_programs"))
            out = w11_codexservi_5c6a93_advertised_cyber_programs(entry);
        else if (!strcmp(func,"daybreak_program"))
            out = json_string(w11_codexservi_5c6a93_daybreak_program());
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
