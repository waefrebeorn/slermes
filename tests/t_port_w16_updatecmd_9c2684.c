#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_updatecmd_9c2684.h"

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
        if (!strcmp(func, "print_items")) {
            out = w16_updatecmd_9c2684_print_items(
                json_obj_get(in, "items"), json_get_str(in, "label", ""),
                json_get_str(in, "key", ""),
                json_has(in, "fallback_key") ? json_get_str(in, "fallback_key", "") : NULL);
        } else if (!strcmp(func, "ask")) {
            json_t *ir = json_obj_get(in, "input_reply");
            const char *input_reply = (ir && ir->type == JSON_STRING) ? ir->str_val : NULL;
            out = w16_updatecmd_9c2684_ask_configure_new_options(
                json_get_bool(in, "assume_yes", false),
                json_get_bool(in, "gateway_mode", false),
                json_get_str(in, "gateway_reply", ""),
                input_reply,
                json_get_bool(in, "is_interactive", false));
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
