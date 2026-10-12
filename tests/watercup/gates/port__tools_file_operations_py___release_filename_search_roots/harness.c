/* gate: port__tools_file_operations_py___release_filename_search_roots — traced copy of _release_filename_search_roots (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _release_filename_search_roots(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _release_filename_search_roots <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_release_filename_search_roots");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _release_filename_search_roots(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_release_filename_search_roots");
    }
    return 0;
}
