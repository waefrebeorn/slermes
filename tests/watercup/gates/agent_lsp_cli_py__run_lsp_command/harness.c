/* gate: agent_lsp_cli_py__run_lsp_command — traced copy of lspc2_run_lsp_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc2_run_lsp_command(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("lspc2_run_lsp_command");
    int result = lspc2_run_lsp_command(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc2_run_lsp_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc2_run_lsp_command <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
