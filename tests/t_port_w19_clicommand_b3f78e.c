#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_clicommand_b3f78e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return def;
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
        if (!strcmp(func, "keep")) {
            out = w19_clicommand_b3f78e_keep(json_obj_get(in, "lines"));
        } else if (!strcmp(func, "prepare_session")) {
            out = w19_clicommand_b3f78e_prepare_session(
                jbool(in, "agent_running", 0), jbool(in, "store_present", 0),
                jbool(in, "session_db_present", 0), jbool(in, "session_exists", 0),
                jbool(in, "set_title_raises", 0), jbool(in, "get_title_raises", 0),
                json_get_str(in, "session_id", "abcdef1234567890"),
                json_get_str(in, "title_value", ""));
        } else if (!strcmp(func, "validate_target")) {
            out = w19_clicommand_b3f78e_validate_target(
                json_get_str(in, "platform_name", "telegram"),
                json_get_str(in, "outcome", "ok"), json_get_str(in, "home_name", ""));
        } else if (!strcmp(func, "handoff_wait")) {
            out = w19_clicommand_b3f78e_handoff_wait(
                json_obj_get(in, "states"), jbool(in, "fail_pending_raises", 0),
                json_get_str(in, "platform_name", "telegram"),
                json_get_str(in, "session_title", "handoff-abcdef12"));
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
