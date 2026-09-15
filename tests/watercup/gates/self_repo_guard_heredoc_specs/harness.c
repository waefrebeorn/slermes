/* gate: self_repo_guard_heredoc_specs
 * Calls the REAL C port src/tools/self_repo_guard.c::srg_heredoc_specs
 * (linked from the repo's own object files) and traces its per-spec result
 * shape against LIVE Python tools/self_repo_guard.py::_heredoc_specs.
 * Input argv: one shell line per argument.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

typedef struct { char *delimiter; int strip_tabs; int execute_as_shell; } srg_hd_t;
srg_hd_t **srg_heredoc_specs(const char *line);

/* out-of-path externals (not exercised by srg_heredoc_specs' traced path) */
void audit_log_security(const char *fmt, ...){(void)fmt;}
int apr_has_allowlist_shell_operator(const char *a){(void)a;return 0;}
int tirith_inline_scan(const char *a){(void)a;return 0;}
int url_is_safe(const char *a){(void)a;return 1;}
void gw_approval_set_context(void){}
void hermes_config_load(void){}
void registry_register(void){}
void hermes_log(int lvl,const char *fmt,...){(void)lvl;(void)fmt;}

static void run_one(const char *line) {
    WCUP_BEGIN("heredoc_specs");
    srg_hd_t **specs = srg_heredoc_specs(line);
    int n = 0;
    while (specs && specs[n]) n++;
    WCUP_BRANCH(n > 0);
    for (int i = 0; i < n; i++) {
        WCUP_LOOP_TICK();
        WCUP_BRANCH(specs[i]->strip_tabs != 0);
        WCUP_BRANCH(specs[i]->execute_as_shell != 0);
        WCUP_VALSTR(specs[i]->delimiter ? specs[i]->delimiter : "");
    }
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("heredoc_specs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s heredoc_specs <line>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
