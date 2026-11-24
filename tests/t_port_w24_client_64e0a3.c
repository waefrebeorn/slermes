/* JSON-lines harness for the w24_client_64e0a3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_client_64e0a3.h"

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
        if (!strcmp(func, "parse_body")) {
            out = w24_client_64e0a3_parse_body(in);
        } else if (!strcmp(func, "retry_after")) {
            out = w24_client_64e0a3_retry_after(in);
        } else if (!strcmp(func, "base_url")) {
            out = w24_client_64e0a3_base_url(in);
        } else if (!strcmp(func, "instance_id")) {
            out = w24_client_64e0a3_instance_id(in);
        } else if (!strcmp(func, "response_error")) {
            out = w24_client_64e0a3_response_error(in);
        } else if (!strcmp(func, "response_message")) {
            out = w24_client_64e0a3_response_message(in);
        } else if (!strcmp(func, "constants")) {
            out = w24_client_64e0a3_constants();
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
