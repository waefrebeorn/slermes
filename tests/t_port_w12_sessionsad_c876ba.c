/* t_port_w12_sessionsad_c876ba.c — JSON-lines harness for hermes_cli/sessions_adopt.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_sessionsad_c876ba.h"

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
        char buf[4096];
        if (!strcmp(func, "refusal")) {
            w12_sessionsad_c876ba_refusal(
                json_get_str(in, "reason", ""),
                json_get_str(in, "field", NULL),
                json_get_str(in, "detail", NULL), buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "unreadable_store")) {
            w12_sessionsad_c876ba_unreadable_store(
                json_get_str(in, "kind", ""),
                json_get_str(in, "store", ""),
                json_get_str(in, "exc_text", ""), buf, sizeof(buf));
            out = json_string(buf);
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
