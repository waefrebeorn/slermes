/* t_port_w11_slashworke_1620bb.c — harness for the slash_worker C port. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_slashworke_1620bb.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *own_str(char *s) {
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
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
        if (!strcmp(func, "slash_base")) {
            out = own_str(w11_slashworke_1620bb_slash_base(json_get_str(in, "command", "")));
        } else if (!strcmp(func, "parse_line")) {
            out = w11_slashworke_1620bb_parse_line(json_get_str(in, "raw", ""));
        } else if (!strcmp(func, "skill_refused")) {
            out = own_str(w11_slashworke_1620bb_skill_refused_message(
                json_get_str(in, "base", "")));
        } else if (!strcmp(func, "reply")) {
            out = own_str(w11_slashworke_1620bb_reply(json_obj_get(in, "fields")));
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
