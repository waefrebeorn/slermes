/* gate: display_split_shell_compound — links the REAL port
 * src/hermes_cli/display.c::cli_agent_display__split_shell_compound and
 * traces its segmentation decisions.
 * LIVE twin: agent/display.py::_split_shell_compound.
 * Trace contract (same rung order both sides):
 *   B1 command non-empty; then per emitted segment B(1)+V(segment);
 *   terminator B(0) after the final segment.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

char **cli_agent_display__split_shell_compound(const char *command, int *count);

static void trace_split(const char *command) {
    WCUP_BEGIN("split_shell_compound");
    int has = command && *command;
    WCUP_BRANCH(has);
    if (!has) {
        WCUP_VALSTR("");
    } else {
        char **segs = NULL; int n = 0;
        /* real C port */
        segs = cli_agent_display__split_shell_compound(command, &n);
        for (int i = 0; i < n; i++) {
            WCUP_BRANCH(1);
            WCUP_VALSTR(segs[i]);
        }
        WCUP_BRANCH(0);
    }
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("split_shell_compound");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s split_shell_compound <cmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_split(argv[i]);
    return 0;
}
