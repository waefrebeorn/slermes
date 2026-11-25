/* t_port_w18_processreg_e1509d.c — harness for src/tools/process_registry_access.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_processreg_e1509d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
/* session fields live under the "session" object. */
static const char *sget(const json_t *in, const char *k) {
    return json_get_str(json_obj_get(in, "session"), k, "");
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *owns = json_obj_get(in, "owns");
        json_t *out = NULL;
        if (!strcmp(func, "session_action_allowed")) {
            out = json_bool(w18_processreg_e1509d_session_action_allowed(
                sget(in, "owner_task_id"), sget(in, "task_id"), sget(in, "session_key"),
                json_get_str(in, "caller_task", ""), json_get_str(in, "caller_session", ""), owns));
        } else if (!strcmp(func, "resolve_actionable")) {
            out = w18_processreg_e1509d_resolve_actionable(
                json_get_str(in, "session_id", ""), json_get_str(in, "task_id", ""),
                json_get_bool(in, "exists", 1),
                sget(in, "owner_task_id"), sget(in, "task_id"), sget(in, "session_key"),
                json_get_str(in, "full_id", ""), json_get_str(in, "caller_key", ""), owns);
        } else
            out = json_string("UNKNOWN_FUNC");
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
