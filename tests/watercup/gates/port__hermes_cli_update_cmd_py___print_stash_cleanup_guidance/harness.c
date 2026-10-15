/* gate: port__hermes_cli_update_cmd_py___print_stash_cleanup_guidance — traced copy of uc_stash_cleanup_guidance (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * uc_stash_cleanup_guidance(const char *stash_ref,
                                    const char *stash_selector);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_stash_cleanup_guidance <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_stash_cleanup_guidance");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = uc_stash_cleanup_guidance(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_stash_cleanup_guidance");
    }
    return 0;
}
