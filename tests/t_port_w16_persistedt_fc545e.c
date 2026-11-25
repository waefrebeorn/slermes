/* t_port_w16_persistedt_fc545e.c — JSON-lines harness for persisted_turn_receipt.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_persistedt_fc545e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "committed_row_id")) {
            out = w16_persistedt_fc545e_committed_row_id(json_obj_get(in, "message"));
        } else if (!strcmp(func, "receipt")) {
            out = w16_persistedt_fc545e_receipt(json_obj_get(in, "messages"),
                    json_get_str(in, "start", "0"), json_obj_get(in, "history"),
                    json_get_str(in, "raw", ""), json_get_str(in, "status", ""),
                    json_get_bool(in, "compression_unchanged", 0) ? 1 : 0);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
