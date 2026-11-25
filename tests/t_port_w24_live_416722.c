/* JSON-lines harness for the w24_live_416722 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_live_416722.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "effective_profile_key")) {
            out = w24_live_416722_effective_profile_key(json_obj_get(in, "profile_home"),
                                                        json_obj_get(in, "process_key"));
        } else if (!strcmp(func, "key_pair")) {
            out = w24_live_416722_key_pair(json_obj_get(in, "session_key"),
                                           json_obj_get(in, "profile_home"),
                                           json_obj_get(in, "process_key"));
        } else if (!strcmp(func, "select")) {
            out = w24_live_416722_select(json_obj_get(in, "ops"), in);
        } else if (!strcmp(func, "open_result")) {
            out = w24_live_416722_open_result(json_obj_get(in, "ops"), in);
        } else if (!strcmp(func, "close_result")) {
            out = w24_live_416722_close_result(json_obj_get(in, "ops"), in);
        } else if (!strcmp(func, "conflict_message")) {
            out = w24_live_416722_conflict_message(json_obj_get(in, "session_key"),
                                                   json_obj_get(in, "op_id"));
        } else if (!strcmp(func, "constants")) {
            out = w24_live_416722_constants();
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
        json_free(in);
    }
    return 0;
}
