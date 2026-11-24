/* t_port_w17_gatewayaut_143263.c — JSON-lines harness for gateway_automation.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_gatewayaut_143263.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "decode")) {
            out = w17_gatewayaut_143263_decode(json_obj_get(in, "value"));
        } else if (!strcmp(func, "gateway_connection")) {
            const json_t *cv = json_obj_get(in, "connection");
            out = w17_gatewayaut_143263_gateway_connection(
                    json_get_bool(in, "is_store", 0) ? 1 : 0,
                    json_get_bool(in, "open", 0) ? 1 : 0,
                    cv && cv->type == JSON_STRING ? cv->str_val : NULL);
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
