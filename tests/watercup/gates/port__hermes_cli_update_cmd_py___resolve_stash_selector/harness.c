/* gate: port__hermes_cli_update_cmd_py___resolve_stash_selector — traced copy of uc_resolve_stash_selector (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * uc_resolve_stash_selector(const char *stash_list_output,
                                    const char *stash_ref);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_resolve_stash_selector <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_resolve_stash_selector");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = uc_resolve_stash_selector(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_resolve_stash_selector");
    }
    return 0;
}
