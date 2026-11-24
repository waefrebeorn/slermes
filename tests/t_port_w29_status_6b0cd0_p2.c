/* JSON-lines harness for the w29_status_6b0cd0_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_status_6b0cd0_p2.h"

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
        if (!strcmp(func, "signed_out")) {
            out = w29_status_6b0cd0_signed_out(in);
        } else if (!strcmp(func, "signed_out_filter")) {
            out = w29_status_6b0cd0_signed_out_filter(json_obj_get(in, "status"));
        } else if (!strcmp(func, "active_sessions")) {
            out = w29_status_6b0cd0_active_sessions(in);
        } else if (!strcmp(func, "feature_state")) {
            out = w29_status_6b0cd0_feature_state(json_obj_get(in, "feat"));
        } else if (!strcmp(func, "portal_status")) {
            out = w29_status_6b0cd0_portal_status(in);
        } else if (!strcmp(func, "log_query")) {
            out = w29_status_6b0cd0_log_query(in);
        } else if (!strcmp(func, "debug_share")) {
            out = w29_status_6b0cd0_debug_share(in);
        } else if (!strcmp(func, "retirement")) {
            out = w29_status_6b0cd0_retirement(in);
        } else if (!strcmp(func, "curator_status")) {
            out = w29_status_6b0cd0_curator_status(in);
        } else if (!strcmp(func, "local_reads")) {
            out = w29_status_6b0cd0_local_reads(in);
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
