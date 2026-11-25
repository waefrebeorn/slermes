#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_handofftarg_ecfd33.h"

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
        int relay_enabled = json_get_bool(in, "relay_enabled", false) ? 1 : 0;
        if (!strcmp(func, "resolve")) {
            out = handofftarg_resolve_handoff_home(
                json_get_str(in, "platform_name", ""),
                json_obj_get(in, "known_platforms"),
                json_obj_get(in, "platforms"),
                json_obj_get(in, "homes"),
                relay_enabled, json_obj_get(in, "relay_names"));
        } else if (!strcmp(func, "relay_fronts")) {
            out = json_bool(handofftarg_relay_fronts(
                relay_enabled, json_obj_get(in, "relay_names"),
                json_get_str(in, "platform_name", "")));
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
