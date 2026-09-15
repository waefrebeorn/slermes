/* gate: hermes_cli_main_py___try_restart_systemd_service — traced copy of main_g_try_restart_systemd_service */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_g_try_restart_systemd_service(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_g_try_restart_systemd_service");
    int result = main_g_try_restart_systemd_service(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_try_restart_systemd_service");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_try_restart_systemd_service <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
