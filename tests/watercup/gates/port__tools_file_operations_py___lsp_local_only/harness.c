/* gate: port__tools_file_operations_py___lsp_local_only — traced copy of fo_lsp_local_only (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool fo_lsp_local_only(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_lsp_local_only <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_lsp_local_only");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fo_lsp_local_only();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_lsp_local_only");
    }
    return 0;
}
