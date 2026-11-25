/* JSON-lines harness for the w31_shared_97a29d_p1 gate (upstream
 * gateway/platforms/_shared.py ingress primitives).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_shared_97a29d_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "secrets_match")) {
            out = w31_shared_97a29d_p1_secrets_match(json_obj_get(in, "provided"),
                                                     json_obj_get(in, "expected"));
        } else if (!strcmp(func, "timestamp_fresh")) {
            out = w31_shared_97a29d_p1_timestamp_fresh(json_obj_get(in, "raw"),
                                                       json_obj_get(in, "window"),
                                                       json_obj_get(in, "now"));
        } else if (!strcmp(func, "decode_json_list_literal")) {
            out = w31_shared_97a29d_p1_decode_json_list_literal(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "coerce_port")) {
            out = w31_shared_97a29d_p1_coerce_port(json_obj_get(in, "value"),
                                                   json_obj_get(in, "default"));
        } else if (!strcmp(func, "bridged_env_marker")) {
            out = w31_shared_97a29d_p1_bridged_env_marker(json_obj_get(in, "name"));
        } else if (!strcmp(func, "request_never_left")) {
            out = w31_shared_97a29d_p1_request_never_left(json_obj_get(in, "chain"));
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
        json_free(in);
    }
    return 0;
}
