/* JSON-lines harness for the w31_gatewayses_1bd734_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_gatewayses_1bd734_p2.h"

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
        if (!strcmp(func, "expand")) {
            out = w31_gatewayses_1bd734_p2_expand(json_obj_get(in, "requested"),
                                                  json_obj_get(in, "has_column"));
        } else if (!strcmp(func, "labelled")) {
            out = w31_gatewayses_1bd734_p2_labelled(json_obj_get(in, "requested"),
                                                    json_obj_get(in, "has_column"),
                                                    json_obj_get(in, "limit"),
                                                    json_obj_get(in, "offset"));
        } else if (!strcmp(func, "banner_inventory")) {
            out = w31_gatewayses_1bd734_p2_banner_inventory(json_obj_get(in, "current"));
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
