/* JSON-lines harness for the w31_gatewayses_1bd734_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_gatewayses_1bd734_p1.h"

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
        if (!strcmp(func, "session_label")) {
            out = w31_gatewayses_1bd734_p1_session_label(json_obj_get(in, "session_id"));
        } else if (!strcmp(func, "window")) {
            out = w31_gatewayses_1bd734_p1_window(json_obj_get(in, "limit"),
                                                  json_obj_get(in, "offset"));
        } else if (!strcmp(func, "transcript_view")) {
            out = w31_gatewayses_1bd734_p1_transcript_view(json_obj_get(in, "include_inactive"),
                                                           json_obj_get(in, "include_compacted"));
        } else if (!strcmp(func, "store_truthy")) {
            out = w31_gatewayses_1bd734_p1_store_truthy(json_obj_get(in, "final"));
        } else if (!strcmp(func, "attr_dispatch")) {
            out = w31_gatewayses_1bd734_p1_attr_dispatch(json_obj_get(in, "name"));
        } else if (!strcmp(func, "retry_state")) {
            out = w31_gatewayses_1bd734_p1_retry_state(json_obj_get(in, "retry"));
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
