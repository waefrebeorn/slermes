/* gate: tools_delegate_tool_py__interrupt_subagent — traced copy of dlg_interrupt_subagent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dlg_interrupt_subagent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dlg_interrupt_subagent");
    int result = dlg_interrupt_subagent(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dlg_interrupt_subagent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_interrupt_subagent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
