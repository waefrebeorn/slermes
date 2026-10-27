/* gate: port__hermes_cli_update_cmd_py___print_parked_branch_skip_warning — traced copy of upd_print_parked_branch_skip_warning (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void upd_print_parked_branch_skip_warning(const char *cwd,
                                          const char *current_branch,
                                          const char *target_branch,
                                          const char *reason);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_print_parked_branch_skip_warning <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("upd_print_parked_branch_skip_warning");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    upd_print_parked_branch_skip_warning();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("upd_print_parked_branch_skip_warning");
    }
    return 0;
}
