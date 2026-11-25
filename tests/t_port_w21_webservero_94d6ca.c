/* JSON-lines harness for the w21_webservero_94d6ca gate (owner-exit watchdog).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_webservero_94d6ca.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "default_poll")) {
            out = json_number(w21_webservero_94d6ca_default_poll());
        } else if (!strcmp(func, "should_retire")) {
            out = w21_webservero_94d6ca_should_retire(json_obj_get(in, "lock"),
                                                      str_or(in, "my_nonce", ""),
                                                      json_get_num(in, "age_s", 0));
        } else if (!strcmp(func, "observe_message")) {
            char *s = w21_webservero_94d6ca_observe_message(str_or(in, "nonce", ""),
                                                            json_obj_get(in, "spawn_nonce"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "simulate")) {
            out = w21_webservero_94d6ca_simulate(in);
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
