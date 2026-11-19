/* gate: port__hermes_cli_update_cmd_py___wait_for_service_active — traced copy of uc_wait_for_service_active (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int uc_wait_for_service_active(const char *service_name,
                                     double timeout_sec,
                                     int poll_ms);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_wait_for_service_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_wait_for_service_active");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = uc_wait_for_service_active(argv[i], strtod(argv[2 + (i - 2 + 1) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_wait_for_service_active");
    }
    return 0;
}
