/* JSON-lines harness for the w20_credential_af0c5f gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_credential_af0c5f.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "serving")) {
            out = w20_credential_af0c5f_serving(json_obj_get(in, "entries"),
                                               json_get_str(in, "endpoint_url", NULL));
        } else if (!strcmp(func, "strategy_pick")) {
            out = w20_credential_af0c5f_strategy_pick(json_get_str(in, "strategy", ""),
                                                      json_obj_get(in, "entries"),
                                                      (int)json_get_num(in, "rng_index", 0));
        } else if (!strcmp(func, "record_pick")) {
            out = w20_credential_af0c5f_record_pick(json_get_str(in, "strategy", ""),
                                                    json_obj_get(in, "entries"),
                                                    json_get_str(in, "picked_id", ""),
                                                    json_get_bool(in, "count", 1) ? 1 : 0);
        } else if (!strcmp(func, "select_unlocked")) {
            out = w20_credential_af0c5f_select_unlocked(json_obj_get(in, "available"),
                                                        json_obj_get(in, "pending"),
                                                        json_get_str(in, "strategy", ""),
                                                        json_get_str(in, "endpoint_url", NULL),
                                                        json_get_bool(in, "count", 1) ? 1 : 0,
                                                        (int)json_get_num(in, "rng_index", 0));
        } else if (!strcmp(func, "peek")) {
            out = w20_credential_af0c5f_peek(json_obj_get(in, "current"),
                                             json_obj_get(in, "available"),
                                             json_obj_get(in, "model"),
                                             json_get_str(in, "endpoint_url", NULL));
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
