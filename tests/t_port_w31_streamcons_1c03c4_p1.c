/* JSON-lines harness for the w31_streamcons_1c03c4_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_streamcons_1c03c4_p1.h"

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
        if (!strcmp(func, "constants")) {
            out = w31_streamcons_1c03c4_p1_constants();
        } else if (!strcmp(func, "draft_metadata")) {
            out = w31_streamcons_1c03c4_p1_draft_metadata(in);
        } else if (!strcmp(func, "stale_preview_ids")) {
            out = w31_streamcons_1c03c4_p1_stale_preview_ids(in);
        } else if (!strcmp(func, "has_real_preview")) {
            out = json_bool(w31_streamcons_1c03c4_p1_has_real_preview(in));
        } else if (!strcmp(func, "adopt_message_id")) {
            out = w31_streamcons_1c03c4_p1_adopt_message_id(in);
        } else if (!strcmp(func, "close_native_state")) {
            out = w31_streamcons_1c03c4_p1_close_native_state(in);
        } else if (!strcmp(func, "enter_fallback_mode")) {
            out = w31_streamcons_1c03c4_p1_enter_fallback_mode(in);
        } else if (!strcmp(func, "track_preview_id")) {
            out = w31_streamcons_1c03c4_p1_track_preview_id(in);
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
