/* t_port_w16_slashcomma_6b9c63.c — JSON-lines harness for gateway/slash_commands_login.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_slashcomma_6b9c63.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "paired_dm"))
            out = json_bool(w16_slashcomma_6b9c63_paired_dm(json_obj_get(in, "platform"),
                                          json_get_str(in, "chat_type", NULL),
                                          json_obj_get(in, "chat_id")) ? 1 : 0);
        else if (!strcmp(func, "attempt_key"))
            out = w16_slashcomma_6b9c63_attempt_key(json_obj_get(in, "platform"),
                                  json_obj_get(in, "chat_id"),
                                  json_obj_get(in, "user_id"));
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
