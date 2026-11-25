/*
 * t_port_w6_turnauthor_9c0bab.c — Oracle harness for agent/turn_author.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_turnauthor_9c0bab.h"

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
        char *s = NULL;
        if (!strcmp(func, "env_key")) {
            out = json_string(w6_turnauthor_9c0bab_env_key());
        } else if (!strcmp(func, "clean_text")) {
            s = w6_turnauthor_9c0bab_clean_text(json_get_str(in, "value", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "bot_flag")) {
            out = json_bool(w6_turnauthor_9c0bab_bot_flag(json_obj_get(in, "value")) ? 1 : 0);
        } else if (!strcmp(func, "bot_author_id")) {
            s = w6_turnauthor_9c0bab_bot_author_id(
                json_get_str(in, "profile", NULL),
                json_has(in, "origin") ? json_get_str(in, "origin", NULL) : NULL);
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "parse")) {
            out = w6_turnauthor_9c0bab_parse_turn_author(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "env")) {
            out = w6_turnauthor_9c0bab_turn_author_env(json_obj_get(in, "author"));
        } else if (!strcmp(func, "a2a_key")) {
            s = w6_turnauthor_9c0bab_a2a_key(json_obj_get(in, "author"));
            out = s ? json_string(s) : json_null();
            free(s);
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
