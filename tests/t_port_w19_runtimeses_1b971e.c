#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_runtimeses_1b971e.h"

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
        if (!strcmp(func, "handoff_rows")) {
            out = w19_runtimeses_1b971e_handoff_rows(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "cas_rows")) {
            out = w19_runtimeses_1b971e_cas_rows(json_obj_get(in, "held"));
        } else if (!strcmp(func, "declared_scope_identity")) {
            out = w19_runtimeses_1b971e_declared_scope_identity(json_obj_get(in, "identity"));
        } else if (!strcmp(func, "is_explicit_fork_child")) {
            out = w19_runtimeses_1b971e_is_explicit_fork_child(json_obj_get(in, "identity"));
        } else if (!strcmp(func, "receipt_journal")) {
            out = w19_runtimeses_1b971e_receipt_journal(json_obj_get(in, "candidate"),
                                                        json_obj_get(in, "result"),
                                                        json_obj_get(in, "scope"));
        } else if (!strcmp(func, "fallback_streak")) {
            out = w19_runtimeses_1b971e_fallback_streak(json_obj_get(in, "streak"));
        } else if (!strcmp(func, "ineffective_count")) {
            out = w19_runtimeses_1b971e_ineffective_count(json_obj_get(in, "count"));
        } else if (!strcmp(func, "recovery_deadline")) {
            out = w19_runtimeses_1b971e_recovery_deadline(json_obj_get(in, "deadline"));
        } else if (!strcmp(func, "session_readable")) {
            out = w19_runtimeses_1b971e_session_readable(json_get_str(in, "scope_session_id", ""),
                                                         json_get_str(in, "requested", ""),
                                                         json_obj_get(in, "readable_ids"));
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
