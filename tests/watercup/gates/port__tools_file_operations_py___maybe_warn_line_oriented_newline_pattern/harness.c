/* gate: port__tools_file_operations_py___maybe_warn_line_oriented_newline_pattern — traced copy of file_pagination_ops_maybe_warn_line_oriented_newline_pattern (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int file_pagination_ops_maybe_warn_line_oriented_newline_pattern(const char *pattern);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_pagination_ops_maybe_warn_line_oriented_newline_pattern <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("file_pagination_ops_maybe_warn_line_oriented_newline_pattern");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = file_pagination_ops_maybe_warn_line_oriented_newline_pattern(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("file_pagination_ops_maybe_warn_line_oriented_newline_pattern");
    }
    return 0;
}
