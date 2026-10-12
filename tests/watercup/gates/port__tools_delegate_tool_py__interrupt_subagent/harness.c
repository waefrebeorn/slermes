/* gate: port__tools_delegate_tool_py__interrupt_subagent — traced copy of dlg_interrupt_subagent (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int dlg_interrupt_subagent(const char *subagent_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_interrupt_subagent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dlg_interrupt_subagent");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = dlg_interrupt_subagent(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dlg_interrupt_subagent");
    }
    return 0;
}
