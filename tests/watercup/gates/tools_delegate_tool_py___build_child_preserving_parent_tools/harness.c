/* gate: tools_delegate_tool_py___build_child_preserving_parent_tools — traced copy of dlg_build_child_preserving_parent_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dlg_build_child_preserving_parent_tools(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dlg_build_child_preserving_parent_tools");
    int result = dlg_build_child_preserving_parent_tools(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dlg_build_child_preserving_parent_tools");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_build_child_preserving_parent_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
