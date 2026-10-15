/* gate: port__hermes_cli_main_py___parse_dashboard_runtime — traced copy of main_g_parse_dashboard_runtime (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int main_g_parse_dashboard_runtime(const char *command, char *mode_out, size_t mode_sz,
                                   char *host_out, size_t host_sz, int *port_out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_parse_dashboard_runtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("main_g_parse_dashboard_runtime");
    char _s1[256]; _s1[0] = 0;
    char _s3[256]; _s3[0] = 0;
    int _o5 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = main_g_parse_dashboard_runtime(argv[i], _s1, sizeof(_s1), _s3, sizeof(_s3), &_o5);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s1);
    WCUP_VALSTR(_s3);
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o5); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("main_g_parse_dashboard_runtime");
    }
    return 0;
}
