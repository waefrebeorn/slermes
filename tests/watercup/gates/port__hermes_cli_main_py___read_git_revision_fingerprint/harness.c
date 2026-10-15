/* gate: port__hermes_cli_main_py___read_git_revision_fingerprint — traced copy of read_git_revision_fingerprint (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * read_git_revision_fingerprint(const char *repo_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s read_git_revision_fingerprint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("read_git_revision_fingerprint");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = read_git_revision_fingerprint(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("read_git_revision_fingerprint");
    }
    return 0;
}
