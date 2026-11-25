/* JSON-lines harness for the w25_delegateto_a1fd91 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_delegateto_a1fd91.h"

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
        if (!strcmp(func, "route_key")) {
            out = w25_delegateto_a1fd91_route_key(json_obj_get(in, "child"));
        } else if (!strcmp(func, "caches_prompts")) {
            out = w25_delegateto_a1fd91_caches_prompts(json_obj_get(in, "child"));
        } else if (!strcmp(func, "first_response_started")) {
            out = w25_delegateto_a1fd91_first_response_started(json_obj_get(in, "child"));
        } else if (!strcmp(func, "for_children")) {
            out = w25_delegateto_a1fd91_for_children(json_obj_get(in, "children"),
                                                     json_obj_get(in, "max_wait_s"));
        } else if (!strcmp(func, "follows")) {
            out = w25_delegateto_a1fd91_follows(in);
        } else if (!strcmp(func, "constants")) {
            out = w25_delegateto_a1fd91_constants();
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
