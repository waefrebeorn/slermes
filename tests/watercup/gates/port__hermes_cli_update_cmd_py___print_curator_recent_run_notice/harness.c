/* gate: port__hermes_cli_update_cmd_py___print_curator_recent_run_notice — traced copy of uc_print_curator_recent_run_notice (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * uc_print_curator_recent_run_notice(const char *last_run_at,
                                         const char *shown_at,
                                         const char *summary,
                                         const char *when_is);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_print_curator_recent_run_notice <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_print_curator_recent_run_notice");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = uc_print_curator_recent_run_notice(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_print_curator_recent_run_notice");
    }
    return 0;
}
