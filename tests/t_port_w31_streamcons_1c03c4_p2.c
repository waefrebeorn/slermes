/* JSON-lines harness for the w31_streamcons_1c03c4_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_streamcons_1c03c4_p2.h"

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
        if (!strcmp(func, "should_push_draft")) {
            out = json_bool(w31_streamcons_1c03c4_p2_should_push_draft(in));
        } else if (!strcmp(func, "defer_draft_retry")) {
            out = w31_streamcons_1c03c4_p2_defer_draft_retry(in);
        } else if (!strcmp(func, "should_send_fresh_final")) {
            out = json_bool(w31_streamcons_1c03c4_p2_should_send_fresh_final(in));
        } else if (!strcmp(func, "degrade_native_to_buffered_send")) {
            out = w31_streamcons_1c03c4_p2_degrade_native_to_buffered_send(in);
        } else if (!strcmp(func, "resolve_draft_streaming")) {
            out = json_bool(w31_streamcons_1c03c4_p2_resolve_draft_streaming(in));
        } else if (!strcmp(func, "resolve_native_streaming")) {
            out = json_bool(w31_streamcons_1c03c4_p2_resolve_native_streaming(in));
        } else if (!strcmp(func, "track_preview_ids_from_result")) {
            out = w31_streamcons_1c03c4_p2_track_preview_ids_from_result(in);
        } else if (!strcmp(func, "adapter_prefers_fresh_final")) {
            out = json_bool(w31_streamcons_1c03c4_p2_adapter_prefers_fresh_final(in));
        } else if (!strcmp(func, "hold_ambiguous_first_send")) {
            out = w31_streamcons_1c03c4_p2_hold_ambiguous_first_send(in);
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
