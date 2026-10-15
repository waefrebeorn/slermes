/* gate: port__hermes_cli_kanban_db_py___rotated_log_path — traced copy of kb_rotated_log_path (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * kb_rotated_log_path(const char *log_path, int generation);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kb_rotated_log_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kb_rotated_log_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = kb_rotated_log_path(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kb_rotated_log_path");
    }
    return 0;
}
