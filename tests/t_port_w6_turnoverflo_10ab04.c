/* t_port_w6_turnoverflo_10ab04.c — differential harness for agent/turn_overflow.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_turnoverflo_10ab04.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* 64-bit integers arrive as decimal STRINGS (json numbers are doubles). */
static long long read_i64(const json_t *in, const char *key, long long def) {
    const char *s = json_get_str(in, key, NULL);
    if (s && *s) return strtoll(s, NULL, 10);
    return def;
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

        if (!strcmp(func, "clamp")) {
            long long r = w6_turnoverflo_clamp_output_cap(
                read_i64(in, "available_out", 0),
                read_i64(in, "old_ctx", 0),
                read_i64(in, "request_input_estimate", 0));
            out = json_number((double)r);
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
