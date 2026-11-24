/* JSON-lines harness for the w26_telegramad_942de7 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramad_942de7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "utf16_offsets")) {
            out = w26_telegramad_942de7_utf16_offsets(opt_str(in, "text"));
        } else if (!strcmp(func, "entity_handle")) {
            out = w26_telegramad_942de7_entity_handle(json_get_str(in, "etype", ""),
                                                      opt_str(in, "span"));
        } else if (!strcmp(func, "leading_handles")) {
            out = w26_telegramad_942de7_leading_handles(opt_str(in, "text"),
                                                        json_obj_get(in, "entities"));
        } else if (!strcmp(func, "leading_bot_handles")) {
            out = w26_telegramad_942de7_leading_bot_handles(opt_str(in, "text"),
                                                            json_obj_get(in, "entities"),
                                                            json_obj_get(in, "bots"));
        } else if (!strcmp(func, "command")) {
            out = w26_telegramad_942de7_command_after_address(opt_str(in, "text"),
                                                              opt_str(in, "bot"),
                                                              json_get_bool(in, "has_entities", false));
        } else if (!strcmp(func, "foreign")) {
            out = w26_telegramad_942de7_foreign_command_target(opt_str(in, "text"),
                                                               json_obj_get(in, "entities"),
                                                               opt_str(in, "bot"));
        } else if (!strcmp(func, "clean")) {
            out = w26_telegramad_942de7_clean_trigger(opt_str(in, "text"), opt_str(in, "bot"));
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
