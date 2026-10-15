/* gate: port__hermes_cli_update_cmd_py___print_fts_optimize_available_notice — traced copy of uc_print_fts_optimize_available_notice (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * uc_print_fts_optimize_available_notice(const char *mode, double size_gb,
                                             bool interrupted);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_print_fts_optimize_available_notice <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_print_fts_optimize_available_notice");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = uc_print_fts_optimize_available_notice((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_print_fts_optimize_available_notice");
    }
    return 0;
}
