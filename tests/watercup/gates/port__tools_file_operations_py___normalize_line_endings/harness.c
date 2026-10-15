/* gate: port__tools_file_operations_py___normalize_line_endings — traced copy of file_ops_normalize_line_endings (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * file_ops_normalize_line_endings(const char *text, const char *target);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_ops_normalize_line_endings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("file_ops_normalize_line_endings");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = file_ops_normalize_line_endings(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("file_ops_normalize_line_endings");
    }
    return 0;
}
