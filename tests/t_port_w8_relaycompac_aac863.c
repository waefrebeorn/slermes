/* t_port_w8_relaycompac_aac863.c — harness for agent/relay_compaction.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_relaycompac_aac863.h"

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
        if (!strcmp(func, "turn_target")) {
            out = w8_relaycompac_aac863_turn_target(in);
        } else if (!strcmp(func, "session_target")) {
            out = w8_relaycompac_aac863_session_target(in);
        } else if (!strcmp(func, "resolve_target")) {
            out = w8_relaycompac_aac863_resolve_target(in);
        } else if (!strcmp(func, "target_available")) {
            out = json_bool(w8_relaycompac_aac863_target_available(in) ? true : false);
        } else if (!strcmp(func, "emit_mark")) {
            out = w8_relaycompac_aac863_emit_mark(in);
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
