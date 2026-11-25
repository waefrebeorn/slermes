/* JSON-lines harness for the w28_messaging_ede80b_p3 gate (platform payloads).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_messaging_ede80b_p3.h"

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
        if (!strcmp(func, "env_info")) {
            out = w28_messaging_ede80b_p3_env_info(json_obj_get(in, "key"),
                                                   json_obj_get(in, "info"));
        } else if (!strcmp(func, "count_csv")) {
            out = w28_messaging_ede80b_p3_count_csv(json_obj_get(in, "value"));
        } else if (!strcmp(func, "platform_state")) {
            out = w28_messaging_ede80b_p3_platform_state(in);
        } else if (!strcmp(func, "posture")) {
            out = w28_messaging_ede80b_p3_posture(in);
        } else if (!strcmp(func, "multiplex_conflict")) {
            out = w28_messaging_ede80b_p3_multiplex_conflict(in);
        } else if (!strcmp(func, "test_verdict")) {
            out = w28_messaging_ede80b_p3_test_verdict(in);
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
