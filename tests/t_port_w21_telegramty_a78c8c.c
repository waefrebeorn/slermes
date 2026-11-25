/* JSON-lines harness for the w21_telegramty_a78c8c gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys read as None and never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_telegramty_a78c8c.h"
#include "plugins/platforms/telegram/telegram_typing_gate.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "in_cooldown")) {
            out = w21_telegramty_a78c8c_typing_in_cooldown(json_obj_get(in, "state"),
                                                          json_obj_get(in, "chat_id"),
                                                          json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "may_fire")) {
            out = w21_telegramty_a78c8c_typing_may_fire(json_obj_get(in, "state"),
                                                        json_obj_get(in, "chat_id"),
                                                        json_get_num(in, "now", 0.0));
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
