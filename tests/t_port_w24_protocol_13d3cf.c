/* JSON-lines harness for the w24_protocol_13d3cf gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_protocol_13d3cf.h"

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
        if (!strcmp(func, "nonempty_str")) {
            out = w24_protocol_13d3cf_nonempty_str(json_obj_get(in, "value"));
        } else if (!strcmp(func, "make_request")) {
            out = w24_protocol_13d3cf_make_request(json_obj_get(in, "type"),
                                                   json_obj_get(in, "token"),
                                                   json_obj_get(in, "payload"),
                                                   json_obj_get(in, "req_id"),
                                                   json_obj_get(in, "uuid"));
        } else if (!strcmp(func, "make_response")) {
            out = w24_protocol_13d3cf_make_response(json_obj_get(in, "req_id"),
                                                    json_obj_get(in, "payload"));
        } else if (!strcmp(func, "make_error")) {
            out = w24_protocol_13d3cf_make_error(json_obj_get(in, "req_id"),
                                                 json_obj_get(in, "error"));
        } else if (!strcmp(func, "encode")) {
            out = w24_protocol_13d3cf_encode(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "decode")) {
            out = w24_protocol_13d3cf_decode(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "validate_request")) {
            out = w24_protocol_13d3cf_validate_request(json_obj_get(in, "msg"),
                                                       json_obj_get(in, "expected_token"));
        } else if (!strcmp(func, "constants")) {
            out = w24_protocol_13d3cf_constants();
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
