#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_gatewaycli_0af67a.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "ws")) {
            out = w12_gatewaycli_0af67a_gateway_ws_target(
                    json_get_str(in, "origin", ""), json_get_str(in, "ticket", ""));
        } else if (!strcmp(func, "reason")) {
            out = w12_gatewaycli_0af67a_bounded_reason(json_obj_get(in, "value"));
        } else if (!strcmp(func, "edata")) {
            out = w12_gatewaycli_0af67a_bounded_error_data(json_obj_get(in, "data"));
        } else if (!strcmp(func, "rpcerr")) {
            out = w12_gatewaycli_0af67a_rpc_error(json_obj_get(in, "error"));
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
