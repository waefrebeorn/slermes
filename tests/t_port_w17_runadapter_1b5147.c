#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_runadapter_1b5147.h"

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
        if (!strcmp(func, "message")) {
            out = w17_runadapter_1b5147_unavailable_message(json_get_str(in, "platform_value", ""),
                                                 json_get_bool(in, "retrying", false) ? 1 : 0);
        } else if (!strcmp(func, "contention")) {
            out = json_bool(w17_runadapter_1b5147_is_token_contention(json_get_str(in, "error_code", "")) ? true : false);
        } else if (!strcmp(func, "discount")) {
            out = w17_runadapter_1b5147_discount_suspended_wait(json_obj_get(in, "entries"),
                        json_get_num(in, "waited_from", 0.0),
                        json_get_num(in, "planned", 0.0),
                        json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "delay")) {
            out = w17_runadapter_1b5147_token_contention_reconnect_delay(in);
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
