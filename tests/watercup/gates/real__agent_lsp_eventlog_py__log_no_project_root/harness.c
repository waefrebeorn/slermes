/* gate: real__agent_lsp_eventlog_py__log_no_project_root — GAP sentinel for log_no_project_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "log_no_project_root";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s log_no_project_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("log_no_project_root");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("log_no_project_root");
    }
    return 0;
}
