/* JSON-lines harness for the w29_sessionnot_c045cd_p1 gate (notification
 * routing identity, dedupe keys, budgets).  One request object per stdin line;
 * one response line carrying the computed "out" plus every input key echoed
 * back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_sessionnot_c045cd_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "current_keys")) {
            out = w29_sessionnot_c045cd_p1_current_keys(json_obj_get(in, "sid"),
                                                        json_obj_get(in, "session"));
        } else if (!strcmp(func, "session_matches")) {
            out = w29_sessionnot_c045cd_p1_session_matches(json_obj_get(in, "session"),
                                                           json_obj_get(in, "keys"));
        } else if (!strcmp(func, "requires_owner")) {
            out = w29_sessionnot_c045cd_p1_requires_owner(json_obj_get(in, "evt"));
        } else if (!strcmp(func, "dedup_key")) {
            out = w29_sessionnot_c045cd_p1_dedup_key(json_obj_get(in, "evt"));
        } else if (!strcmp(func, "refusal_backoff")) {
            out = w29_sessionnot_c045cd_p1_refusal_backoff(json_obj_get(in, "session"));
        } else if (!strcmp(func, "loop_route")) {
            out = w29_sessionnot_c045cd_p1_loop_route_is_gateway_chat(json_obj_get(in, "state"));
        } else if (!strcmp(func, "process_owners")) {
            out = w29_sessionnot_c045cd_p1_session_process_owners(json_obj_get(in, "session"));
        } else if (!strcmp(func, "constants")) {
            out = w29_sessionnot_c045cd_p1_constants();
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
