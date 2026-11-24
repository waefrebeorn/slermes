/* JSON-lines harness for the w24_anthropict_2a0525 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_anthropict_2a0525.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "claude_family_version")) {
            out = w24_anthropict_2a0525_claude_family_version(json_obj_get(in, "model"));
        } else if (!strcmp(func, "model_preserves_prior_thinking")) {
            out = w24_anthropict_2a0525_model_preserves_prior_thinking(json_obj_get(in, "model"));
        } else if (!strcmp(func, "thinking_route")) {
            out = w24_anthropict_2a0525_thinking_route(json_obj_get(in, "base_url"),
                                                       json_obj_get(in, "model"));
        } else if (!strcmp(func, "route_replays_prior_thinking")) {
            out = w24_anthropict_2a0525_route_replays_prior_thinking(json_obj_get(in, "route"),
                                                                     json_obj_get(in, "model"));
        } else if (!strcmp(func, "native_preserves_prior_thinking")) {
            out = w24_anthropict_2a0525_native_preserves_prior_thinking(json_obj_get(in, "base_url"),
                                                                        json_obj_get(in, "model"));
        } else if (!strcmp(func, "constants")) {
            out = w24_anthropict_2a0525_constants();
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
        json_free(in);
    }
    return 0;
}
