/* JSON-lines harness for the w20_provider_2f6167 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_provider_2f6167.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "should_preserve_pending_create_key")) {
            out = json_bool(w20_provider_2f6167_should_preserve_pending_create_key(
                                (int)json_get_num(in, "status", 0),
                                json_obj_get(in, "payload")) ? 1 : 0);
        } else if (!strcmp(func, "resolve_config")) {
            out = w20_provider_2f6167_resolve_config(json_obj_get(in, "seams"));
        } else if (!strcmp(func, "headers")) {
            out = w20_provider_2f6167_headers(json_obj_get(in, "config"));
        } else if (!strcmp(func, "release_body")) {
            out = w20_provider_2f6167_release_body();
        } else if (!strcmp(func, "create_payload")) {
            out = w20_provider_2f6167_create_payload(json_get_bool(in, "managed", 0) ? 1 : 0);
        } else if (!strcmp(func, "pending_key_ops")) {
            out = w20_provider_2f6167_pending_key_ops(json_obj_get(in, "ops"));
        } else if (!strcmp(func, "create_session")) {
            out = w20_provider_2f6167_create_session(json_obj_get(in, "seams"),
                                                     json_get_str(in, "task_id", ""),
                                                     json_obj_get(in, "response"));
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
