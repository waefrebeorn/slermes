/* t_port_delegation_pins.c — explicit board-intent pin gate (root seam v2).
 * Reads {"env":{...},"in_process":0|1}; emits env/input + out.
 */
#include "hermes_json.h"
#include "hermes_delegation_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_home_value;
static char *stub_kanban_home(void) {
    return strdup(g_home_value ? g_home_value : "");
}
extern void delegation_context_enter_non_dispatcher_owned(void);
extern void delegation_context_exit_non_dispatcher_owned(void);

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
        json_t *ndj = json_obj_get(in, "non_dispatcher");
        int non_dispatcher = (ndj && (ndj->type == JSON_BOOL ? ndj->bool_val :
                                      ndj->type == JSON_NUMBER ? ndj->num_val != 0 : 0)) ? 1 : 0;
        if (in_process) delegated_child_context_enter();
        if (non_dispatcher) delegation_context_enter_non_dispatcher_owned();
        int out = explicit_board_intent_is_pinned() ? 1 : 0;
        json_t *pathj = json_obj_get(in, "path");
        const char *path = (pathj && pathj->type == JSON_STRING) ? pathj->str_val : "";
        int fenced = kanban_path_is_fenced(path) ? 1 : 0;
        const char *owned = owned_kanban_task();
        json_t *homej = json_obj_get(in, "home");
        json_t *hej = json_obj_get(in, "home_error");
        int home_error = (hej && (hej->type == JSON_BOOL ? hej->bool_val :
                                  hej->type == JSON_NUMBER ? hej->num_val != 0 : 0)) ? 1 : 0;
        g_home_value = (homej && homej->type == JSON_STRING) ? homej->str_val : NULL;
        const char *root = home_error ? "1" : fenced_kanban_root_from(stub_kanban_home);
        if (non_dispatcher) delegation_context_exit_non_dispatcher_owned();
        if (in_process) delegated_child_context_exit();
        fputs("{\"env\":", stdout);
        if (env) { char *s = json_dumps(env, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        fputs(",\"path\":", stdout);
        if (pathj) { char *s = json_dumps(pathj, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        fputs(",\"home\":", stdout);
        if (homej) { char *s = json_dumps(homej, 0); fputs(s ? s : "null", stdout); free(s); }
        else fputs("null", stdout);
        fputs(",\"root\":", stdout);
        { char *s = json_dumps(json_string(root ? root : ""), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        fputs(",\"owned\":", stdout);
        { char *s = json_dumps(json_string(owned ? owned : ""), 0); fputs(s ? s : "\"\"", stdout); free(s); }
        printf(",\"non_dispatcher\":%d,\"in_process\":%d,\"out\":%d,\"fenced\":%d}\n", non_dispatcher, in_process, out, fenced);
        json_free(in);
    }
    return 0;
}
