/* gate: shell_heredoc_mask_simple_quotes — includes the REAL
 * src/tools/shell_heredoc.c (mask_simple_quotes is static there, so the file
 * is #included directly and the REAL shipped function computes the result).
 * LIVE twin: tools/shell_heredoc.py::_mask_simple_quotes.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: command contains a single quote   '
 *   B2: command contains a double quote   "
 *   B3: command contains a backtick       `
 *   V : masked output (computed by the REAL C function)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

#include "../../../../src/tools/shell_heredoc.c"

static void trace_one(const char *cmd) {
    WCUP_BEGIN("mask_simple_quotes");
    WCUP_BRANCH(strchr(cmd, '\'') != NULL);   /* B1 */
    WCUP_BRANCH(strchr(cmd, '"') != NULL);    /* B2 */
    WCUP_BRANCH(strchr(cmd, '`') != NULL);    /* B3 */
    char *masked = mask_simple_quotes(cmd);
    WCUP_VALSTR(masked ? masked : "");
    free(masked);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mask_simple_quotes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mask_simple_quotes <command>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
