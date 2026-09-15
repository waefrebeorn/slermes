/* gate: shell_heredoc_parse_heredoc_operator
 * Exercises the REAL C port src/tools/shell_heredoc.c::parse_heredoc_operator
 * (static — pulled in by textual inclusion) against LIVE Python
 * tools/shell_heredoc.py::_parse_heredoc_operator.
 * Input argv format: "<index><TAB><command>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "shell_heredoc.c"

static void run_one(const char *input) {
    WCUP_BEGIN("parse_heredoc_operator");
    char *tmp = strdup(input);
    char *tab = strchr(tmp, '\t');
    size_t index = 0;
    const char *command = tmp;
    if (tab) { *tab = '\0'; index = (size_t)strtoul(tmp, NULL, 10); command = tab + 1; }

    size_t end = 0; char *delim = NULL; bool strip_tabs = false, quoted = false;
    bool ok = parse_heredoc_operator(command, index, &end, &delim,
                                     &strip_tabs, &quoted);
    WCUP_BRANCH(ok);
    if (ok) {
        WCUP_BRANCH(strip_tabs);
        WCUP_BRANCH(quoted);
        char vbuf[32];
        snprintf(vbuf, sizeof vbuf, "%zu", end);
        WCUP_VALSTR(vbuf);
        WCUP_VALSTR(delim);
    } else {
        WCUP_VALSTR("None");
    }
    free(delim);
    free(tmp);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_heredoc_operator");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_heredoc_operator <idx\\tcmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
