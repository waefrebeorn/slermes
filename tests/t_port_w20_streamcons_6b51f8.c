/* JSON-lines harness for the w20_streamcons_6b51f8 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * an absent text is the empty string, exactly what the predicates see. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_streamcons_6b51f8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "pending_media")) {
            out = json_bool(w20_streamcons_6b51f8_pending_media(json_get_str(in, "text", "")) ? 1 : 0);
        } else if (!strcmp(func, "pending_flag")) {
            char *f = w20_streamcons_6b51f8_pending_flag(json_get_str(in, "text", ""));
            out = f ? json_string(f) : json_null();
            free(f);
        } else if (!strcmp(func, "flag_opens_stream")) {
            out = json_bool(w20_streamcons_6b51f8_flag_opens_stream(json_get_str(in, "text", "")) ? 1 : 0);
        } else if (!strcmp(func, "pending_image")) {
            out = json_bool(w20_streamcons_6b51f8_pending_image(json_get_str(in, "text", "")) ? 1 : 0);
        } else if (!strcmp(func, "has_pending")) {
            out = json_bool(w20_streamcons_6b51f8_has_pending(json_get_str(in, "text", "")) ? 1 : 0);
        } else if (!strcmp(func, "last_line")) {
            char *l = w20_streamcons_6b51f8_last_line(json_get_str(in, "text", ""));
            out = json_string(l ? l : "");
            free(l);
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
