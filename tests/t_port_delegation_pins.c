/* t_port_delegation_pins.c — explicit board-intent pin gate.
 * Reads {"env":{...},"in_process":0|1}; emits env/input + out.
 */
#include "hermes_json.h"
#include "hermes_delegation_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void setenv_json(const json_t *env, const char *key) {
    json_t *v = env ? json_obj_get(env, key) : NULL;
    if (!v || v->type == JSON_NULL) {
        unsetenv(key);
    } else {
        const char *s = (v->type == JSON_STRING && v->str_val) ? v->str_val : "";
        setenv(key, s, 1);
    }
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *env = json_obj_get(in, "env");
        setenv_json(env, "HERMES_DELEGATED_CHILD_CONTEXT");
        setenv_json(env, "HERMES_KANBAN_TASK");
        setenv_json(env, "HERMES_KANBAN_DB");
        json_t *ip = json_obj_get(in, "in_process");
        int in_process = (ip && (ip->type == JSON_BOOL ? ip->bool_val :
                                 ip->type == JSON_NUMBER ? ip->num_val != 0 : 0)) ? 1 : 0;
        if (in_process) delegated_child_context_enter();
        int out = explicit_board_intent_is_pinned() ? 1 : 0;
        json_t *pathj = json_obj_get(in, "path");
        const char *path = (pathj && pathj->type == JSON_STRING) ? pathj->str_val : "";
        int fenced = kanban_path_is_fenced(path) ? 1 : 0;
        if (in_process) delegated_child_context_exit();
        fputs("{\"env\":", stdout);
        if (env) { char *s = json_dumps(env, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        fputs(",\"path\":", stdout);
        if (pathj) { char *s = json_dumps(pathj, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        printf(",\"in_process\":%d,\"out\":%d,\"fenced\":%d}\n", in_process, out, fenced);
        json_free(in);
    }
    return 0;
}
