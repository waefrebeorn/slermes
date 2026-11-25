/* JSON-lines harness for the w24_replay_2d5a10 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_replay_2d5a10.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "markers")) {
            out = w24_replay_2d5a10_has_cache_markers(json_obj_get(in, "api_kwargs"));
        } else if (!strcmp(func, "replayable")) {
            out = w24_replay_2d5a10_is_replayable(json_obj_get(in, "api_kwargs"));
        } else if (!strcmp(func, "cap")) {
            out = w24_replay_2d5a10_output_cap(json_obj_get(in, "model"));
        } else if (!strcmp(func, "bounded")) {
            out = w24_replay_2d5a10_bounded_chat_replay(json_obj_get(in, "api_kwargs"),
                                                        json_obj_get(in, "model"));
        } else if (!strcmp(func, "honoured")) {
            out = w24_replay_2d5a10_cap_honoured(json_obj_get(in, "client"),
                                                 json_obj_get(in, "api_kwargs"),
                                                 json_obj_get(in, "model"),
                                                 json_obj_get(in, "api_mode"));
        } else if (!strcmp(func, "adaptive")) {
            out = w24_replay_2d5a10_adaptive(json_obj_get(in, "model"));
        } else if (!strcmp(func, "constants")) {
            out = w24_replay_2d5a10_constants();
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
