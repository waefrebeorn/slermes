#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_receipts.h"

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

        if (!strcmp(func, "protocol_name")) {
            out = json_string(receipts_protocol_name());
        } else if (!strcmp(func, "is_protocol")) {
            out = json_bool(receipts_is_protocol() ? true : false);
        } else if (!strcmp(func, "method_count")) {
            out = json_number((double)receipts_method_count());
        } else if (!strcmp(func, "method_at")) {
            json_t *iv = json_obj_get(in, "i");
            long idx = 0;
            if (iv && iv->type == JSON_NUMBER) idx = (long)iv->num_val;
            else idx = strtol(json_get_str(in, "i", "0"), NULL, 10);
            const char *v = (idx >= 0) ? receipts_method_at((size_t)idx) : NULL;
            out = v ? json_string(v) : json_null();
        } else if (!strcmp(func, "has_method")) {
            out = json_bool(receipts_has_method(json_get_str(in, "name", "")) ? true : false);
        } else if (!strcmp(func, "stub")) {
            const char *v = receipts_method_stub(json_get_str(in, "name", ""));
            out = v ? json_string(v) : json_null();
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
