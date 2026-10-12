/* gate: port__hermes_cli_update_cmd_py___ensure_acp_launcher — traced copy of uc_ensure_acp_launcher (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool uc_ensure_acp_launcher(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_ensure_acp_launcher <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_ensure_acp_launcher");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = uc_ensure_acp_launcher();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_ensure_acp_launcher");
    }
    return 0;
}
