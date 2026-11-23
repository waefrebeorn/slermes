/* t_port_w8_sessionusag_c7f1bb.c -- JSON-lines harness for src/gateway/session_usage.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessionusag_c7f1bb.h"

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
        if (!strcmp(func, "finite")) {
            out = w8_sessionusag_c7f1bb_finite(json_obj_get(in, "value"));
        } else if (!strcmp(func, "carried_compression_count")) {
            out = w8_sessionusag_c7f1bb_carried_compression_count(
                json_obj_get(in, "live"), json_obj_get(in, "agent"));
        } else if (!strcmp(func, "stored_stats")) {
            out = w8_sessionusag_c7f1bb_stored_stats(json_obj_get(in, "row"));
        } else if (!strcmp(func, "tui_usage")) {
            out = w8_sessionusag_c7f1bb_tui_usage(json_obj_get(in, "stats"));
        } else if (!strcmp(func, "usage_frame")) {
            out = w8_sessionusag_c7f1bb_usage_frame(json_obj_get(in, "stats"));
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
