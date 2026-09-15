/* gate: tools_self_repo_guard_find_mutation
 * Links the REAL C port src/tools/self_repo_guard.c::srg_find_mutation
 * (+ src/tools/approval.o for its shell-word externals) against LIVE
 * tools/self_repo_guard.py::_find_mutation.
 *
 * Fixed context (identical on both sides): cwd=/tmp, root=<dev checkout>.
 * Input argv token: "<command>" (single token, no tabs/newlines).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1 empty command        B2 mutation detected
 * Value: operation string or "" for None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *srg_find_mutation(const char *command, const char *cwd,
                        const char *root, int depth);

static const char *SRG_CWD = "/tmp";
static const char *SRG_ROOT = "/home/wubu/hermes-agent-dev";

/* out-of-path externals from linked objects (not on the traced path) */
void audit_log_security(const char *fmt, ...){(void)fmt;}
int apr_has_allowlist_shell_operator(const char *a){(void)a;return 0;}
int tirith_inline_scan(const char *a){(void)a;return 0;}
int url_is_safe(const char *a){(void)a;return 1;}
void gw_approval_set_context(void){}
void hermes_config_load(void){}
void registry_register(void){}
void hermes_log(int lvl,const char *fmt,...){(void)lvl;(void)fmt;}

void run_one(const char *cmd) {
    WCUP_BEGIN("find_mutation");
    int empty = (cmd == NULL || *cmd == '\0');
    WCUP_BRANCH(empty);                                        /* B1 */
    char *op = NULL;
    if (!empty)
        op = srg_find_mutation(cmd, SRG_CWD, SRG_ROOT, 0);
    WCUP_BRANCH(op != NULL);                                   /* B2 */
    WCUP_VALSTR(op ? op : "");
    free(op);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("find_mutation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s find_mutation <command>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
