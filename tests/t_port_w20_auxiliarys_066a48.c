/* JSON-lines harness for the w20_auxiliarys_066a48 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_auxiliarys_066a48.h"
#include "../src/agent/auxiliary_stream_watchdog.c"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "stream_windows")) {
            out = w20_auxiliarys_066a48_stream_windows(json_obj_get(in, "spec"));
        } else if (!strcmp(func, "timeout_error")) {
            char *s = w20_auxiliarys_066a48_timeout_error(
                json_get_bool(in, "saw_progress", 0) ? 1 : 0,
                json_get_num(in, "window", 0.0),
                json_obj_get(in, "first_token"),
                json_get_num(in, "elapsed", 0.0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "close_chunk_stream")) {
            const char *kind = w20_auxiliarys_066a48_close_chunk_stream(json_obj_get(in, "spec"));
            out = kind ? json_string(kind) : json_null();
        } else if (!strcmp(func, "simulate")) {
            out = w20_auxiliarys_066a48_simulate(json_obj_get(in, "spec"));
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
