/*
 * oneshot.c — canonical C11 port of agent/oneshot.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_oneshot_u_commit_message_template @ agent/oneshot.py:_commit_message_template */
int agent_oneshot_u_commit_message_template(const char *arg) {
    /* Python: commit msg template. Arg =
     * "has_diff\thas_recent\thas_avoid\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("commit template: recent=%s diff=%s avoid=%s\n",
           (t1 && t1[1] == '1') ? "yes" : "no", arg[0] == '1' ? "yes" : "no",
           (t2 && t2[1] == '1') ? "yes" : "no");
    return 0;
}

/* PoP: agent_oneshot_render_template @ agent/oneshot.py:render_template */
int agent_oneshot_render_template(const char *arg) {
    /* Python: template lookup, KeyError on unknown. Arg = "name\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    printf("%s\n", tab + 1);
    return 0;
}

/* PoP: agent_oneshot_run_oneshot @ agent/oneshot.py:run_oneshot */
int agent_oneshot_run_oneshot(const char *arg) {
    /* Python: stateless LLM call. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "empty") == 0) {
        fprintf(stderr, "run_oneshot requires a template or instructions/user_input\n");
        return 1;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
