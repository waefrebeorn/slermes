/* t_port_w6_turnapical_c014d7.c — differential harness for agent/turn_api_call.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_turnapical_c014d7.h"

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

        if (!strcmp(func, "should_stream")) {
            int r = w6_turnapical_c014d7_should_stream(
                json_get_bool(in, "disable_streaming", 0) ? 1 : 0,
                json_get_str(in, "base_url", ""),
                json_get_str(in, "provider", ""),
                (int)json_get_num(in, "supports_streaming", -1),
                json_get_bool(in, "external_process_provider", 0) ? 1 : 0,
                json_get_bool(in, "has_stream_consumers", 0) ? 1 : 0,
                json_get_bool(in, "client_is_mock", 0) ? 1 : 0);
            out = json_bool(r);
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
