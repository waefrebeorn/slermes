#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_turnproact_a1162e.h"

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
        if (!strcmp(func, "route_offload")) {
            out = w13_turnproact_a1162e_route_offload(
                json_obj_get(in, "window_reason"), json_obj_get(in, "compression_enabled"),
                json_obj_get(in, "has_previous"), json_obj_get(in, "same_clock"),
                json_obj_get(in, "route"), json_obj_get(in, "prev_route"),
                json_obj_get(in, "messages"), json_obj_get(in, "pruned_messages"));
        } else if (!strcmp(func, "offload_cold")) {
            out = w13_turnproact_a1162e_offload_cold(
                json_obj_get(in, "messages"), json_obj_get(in, "compression_enabled"),
                json_obj_get(in, "cache_cold"), json_obj_get(in, "pruned_messages"),
                json_obj_get(in, "pruned_same"));
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
