/* gate: port__tools_file_operations_py___lsp_handles_extension — traced copy of fo_lsp_handles_extension (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool fo_lsp_handles_extension(const char *path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_lsp_handles_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_lsp_handles_extension");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fo_lsp_handles_extension(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_lsp_handles_extension");
    }
    return 0;
}
