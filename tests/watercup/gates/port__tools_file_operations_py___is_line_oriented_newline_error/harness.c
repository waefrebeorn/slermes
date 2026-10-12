/* gate: port__tools_file_operations_py___is_line_oriented_newline_error — traced copy of file_pagination_ops_is_line_oriented_newline_error (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool file_pagination_ops_is_line_oriented_newline_error(const char *error);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_pagination_ops_is_line_oriented_newline_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("file_pagination_ops_is_line_oriented_newline_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = file_pagination_ops_is_line_oriented_newline_error(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("file_pagination_ops_is_line_oriented_newline_error");
    }
    return 0;
}
