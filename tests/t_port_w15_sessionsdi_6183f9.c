#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionsdi_6183f9.h"

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
        if (!strcmp(func, "confirm_prompt")) {
            out = w15_sessionsdi_6183f9_confirm_prompt(json_get_str(in, "session_id", ""),
                                                       json_obj_get(in, "rows"));
        } else if (!strcmp(func, "discard_plan")) {
            out = w15_sessionsdi_6183f9_discard_plan(json_obj_get(in, "snapshot"),
                        json_obj_get(in, "admission"),
                        json_get_bool(in, "yes", false) ? 1 : 0,
                        json_get_bool(in, "isatty", false) ? 1 : 0,
                        json_get_bool(in, "confirm", false) ? 1 : 0);
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
