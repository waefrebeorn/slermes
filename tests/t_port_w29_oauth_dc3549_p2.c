/* JSON-lines harness for the w29_oauth_dc3549_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_oauth_dc3549_p2.h"

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
        if (!strcmp(func, "codex_http_error")) {
            out = w29_oauth_dc3549_p2_codex_http_error(json_obj_get(in, "status"));
        } else if (!strcmp(func, "http_error_detail")) {
            out = w29_oauth_dc3549_p2_http_error_detail(in);
        } else if (!strcmp(func, "codex_start_error")) {
            out = w29_oauth_dc3549_p2_codex_start_error(in);
        } else if (!strcmp(func, "codex_user_code")) {
            out = w29_oauth_dc3549_p2_codex_user_code(json_obj_get(in, "device_data"));
        } else if (!strcmp(func, "codex_exchange")) {
            out = w29_oauth_dc3549_p2_codex_exchange(in);
        } else if (!strcmp(func, "codex_cancelled")) {
            out = w29_oauth_dc3549_p2_codex_cancelled(json_obj_get(in, "sess"));
        } else if (!strcmp(func, "gc_survivors")) {
            out = w29_oauth_dc3549_p2_gc_survivors(in);
        } else if (!strcmp(func, "device_start")) {
            out = w29_oauth_dc3549_p2_device_start(in);
        } else if (!strcmp(func, "validate_profile")) {
            out = w29_oauth_dc3549_p2_validate_profile(json_obj_get(in, "profile"));
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
