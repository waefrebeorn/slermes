/* JSON-lines harness for the w21_hostedroom_3970cb gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_hostedroom_3970cb.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "validate")) {
            out = w21_hostedroom_3970cb_validate_history(
                json_obj_get(in, "value"), json_obj_get(in, "gateway_id"),
                json_obj_get(in, "epoch"));
        } else if (!strcmp(func, "at")) {
            out = w21_hostedroom_3970cb_at_sequence(json_obj_get(in, "history"),
                                                    json_obj_get(in, "seq"));
        } else if (!strcmp(func, "origin")) {
            out = w21_hostedroom_3970cb_origin_gateway_id(json_obj_get(in, "room"));
        } else if (!strcmp(func, "identifier")) {
            out = w21_hostedroom_3970cb_identifier(
                json_obj_get(in, "value"), json_get_str(in, "label", ""),
                json_object_get(in, "max_chars") ? (long long)json_get_num(in, "max_chars", 128.0) : 128,
                json_get_bool(in, "check_shape", 1) ? 1 : 0);
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
