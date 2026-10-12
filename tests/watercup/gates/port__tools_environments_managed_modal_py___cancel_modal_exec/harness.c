/* gate: port__tools_environments_managed_modal_py___cancel_modal_exec — traced copy of cli_tools_environments_managed_modal__cancel_modal_exec (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void cli_tools_environments_managed_modal__cancel_modal_exec(const char *sandbox_id, const char *exec_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_environments_managed_modal__cancel_modal_exec <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_environments_managed_modal__cancel_modal_exec");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_tools_environments_managed_modal__cancel_modal_exec(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_environments_managed_modal__cancel_modal_exec");
    }
    return 0;
}
