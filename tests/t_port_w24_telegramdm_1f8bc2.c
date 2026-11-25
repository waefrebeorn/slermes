/* JSON-lines harness for the w24_telegramdm_1f8bc2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramdm_1f8bc2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    json_free(v);
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
        if (!strcmp(func, "private_chat")) {
            out = w24_telegramdm_1f8bc2_private_chat(json_obj_get(in, "chat_id"));
        } else if (!strcmp(func, "topic_gone")) {
            out = w24_telegramdm_1f8bc2_topic_gone(json_obj_get(in, "chat_id"),
                                                   json_obj_get(in, "error"));
        } else if (!strcmp(func, "dead_topic_may_reroute")) {
            out = w24_telegramdm_1f8bc2_dead_topic_may_reroute(json_obj_get(in, "chat_id"),
                                                               json_obj_get(in, "metadata"));
        } else if (!strcmp(func, "dead_topic_retry_kwargs")) {
            out = w24_telegramdm_1f8bc2_dead_topic_retry_kwargs(json_obj_get(in, "kwargs"));
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
