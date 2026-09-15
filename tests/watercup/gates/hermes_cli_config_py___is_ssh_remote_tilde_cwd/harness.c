/* gate: hermes_cli_config_py___is_ssh_remote_tilde_cwd — traced copy of config_is_ssh_remote_tilde_cwd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_is_ssh_remote_tilde_cwd(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("config_is_ssh_remote_tilde_cwd");
    int result = config_is_ssh_remote_tilde_cwd(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_is_ssh_remote_tilde_cwd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_is_ssh_remote_tilde_cwd <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
