/* tests/t_port_w19_subagents_8f6344.c - JSON-lines harness for persistence/authority/subagents.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_subagents_8f6344.h"

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
        if (!strcmp(func, "parent_superseded_note")) {
            json_t *reason = json_obj_get(in, "tip_end_reason");
            const char *reason_s = (reason && reason->type == JSON_STRING) ? reason->str_val : NULL;
            out = w19_subagents_8f6344_parent_superseded_note(
                reason_s, json_get_bool(in, "tip_exists", 0) ? 1 : 0);
        } else if (!strcmp(func, "owed")) {
            out = w19_subagents_8f6344_owed(json_obj_get(in, "document"));
        } else if (!strcmp(func, "is_automatic_end_reason")) {
            json_t *reason = json_obj_get(in, "reason");
            const char *reason_s = (reason && reason->type == JSON_STRING) ? reason->str_val : NULL;
            out = json_bool(w19_subagents_8f6344_is_automatic_end_reason(reason_s));
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
