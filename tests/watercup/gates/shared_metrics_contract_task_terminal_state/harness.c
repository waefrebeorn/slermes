/* gate: shared_metrics_contract_task_terminal_state — traced harness over the
 * REAL C port src/hermes_cli/shared_metrics_contract.c::smc_task_terminal_state
 * (linked via extra_src). LIVE twin: hermes_cli/observability/shared_metrics_contract.py::task_terminal_state.
 *
 * argv encoding per input: "<reason>,<interrupted 0|1>,<completed 0|1>,<failed 0|1>"
 * (reason must not contain a comma).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *smc_task_terminal_state(const char *turn_exit_reason,
                              bool interrupted,
                              bool completed,
                              bool failed);

static void trace_one(const char *reason, bool interrupted, bool completed, bool failed) {
    WCUP_BEGIN("task_terminal_state");
    /* mirror the C ladder rung-for-rung */
    const char *r = reason ? reason : "";
    char lr[256]; size_t i;
    for (i = 0; r[i] && i < sizeof(lr) - 1; i++) lr[i] = (char)((r[i] >= 'A' && r[i] <= 'Z') ? r[i] + 32 : r[i]);
    lr[i] = '\0';
    /* trim */
    char *s = lr; while (*s == ' ') s++;
    char *e = s + strlen(s); while (e > s && e[-1] == ' ') e--; *e = '\0';

    int rung = -1;
    if (interrupted || strstr(s, "interrupt") || strstr(s, "cancel")) {
        WCUP_BRANCH(1); rung = 0;
    } else {
        WCUP_BRANCH(0);
        if (strstr(s, "timeout") || strstr(s, "timed_out")) { WCUP_BRANCH(1); rung = 1; }
        else {
            WCUP_BRANCH(0);
            if (strstr(s, "max_iterations") || strstr(s, "budget_exhausted")) { WCUP_BRANCH(1); rung = 2; }
            else {
                WCUP_BRANCH(0);
                if (strstr(s, "approval") && (strstr(s, "denied") || strstr(s, "rejected"))) { WCUP_BRANCH(1); rung = 3; }
                else {
                    WCUP_BRANCH(0);
                    if (strstr(s, "guardrail")) { WCUP_BRANCH(1); rung = 4; }
                    else {
                        WCUP_BRANCH(0);
                        if (strcmp(s, "system_aborted") == 0) { WCUP_BRANCH(1); rung = 5; }
                        else {
                            WCUP_BRANCH(0);
                            if (completed) { WCUP_BRANCH(1); rung = 6; }
                            else {
                                WCUP_BRANCH(0);
                                WCUP_BRANCH(failed || (s[0] && strcmp(s, "unknown") != 0));
                                rung = 7;
                            }
                        }
                    }
                }
            }
        }
    }
    (void)rung;
    /* call the REAL port for the value */
    char *res = smc_task_terminal_state(reason, interrupted, completed, failed);
    WCUP_VALSTR(res ? res : "null");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(res);
    WCUP_END("task_terminal_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s task_terminal_state <reason,i,c,f>...\n", argv[0]); return 2; }
    for (int k = 2; k < argc; k++) {
        char buf[512]; snprintf(buf, sizeof buf, "%s", argv[k]);
        char *c1 = strchr(buf, ',');
        if (!c1) { fprintf(stderr, "bad input\n"); return 2; }
        *c1 = '\0';
        char *c2 = strchr(c1 + 1, ','); if (!c2) return 2; *c2 = '\0';
        char *c3 = strchr(c2 + 1, ','); if (!c3) return 2; *c3 = '\0';
        trace_one(buf, c1[1] == '1', c2[1] == '1', c3[1] == '1');
    }
    return 0;
}
