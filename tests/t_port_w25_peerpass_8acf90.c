/* JSON-lines harness for the w25_peerpass_8acf90 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_peerpass_8acf90.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "is_peer_path")) {
            out = w25_peerpass_8acf90_is_peer_path(json_obj_get(in, "path"));
        } else if (!strcmp(func, "peer_access_contract")) {
            out = w25_peerpass_8acf90_peer_access_contract(json_obj_get(in, "providers"));
        } else if (!strcmp(func, "verify")) {
            out = w25_peerpass_8acf90_verify(json_obj_get(in, "providers"),
                                             json_obj_get(in, "token"));
        } else if (!strcmp(func, "constants")) {
            out = w25_peerpass_8acf90_constants();
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
        json_free(in);
    }
    return 0;
}
