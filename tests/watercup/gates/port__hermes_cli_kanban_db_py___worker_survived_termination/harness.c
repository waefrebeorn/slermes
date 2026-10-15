/* gate: port__hermes_cli_kanban_db_py___worker_survived_termination — traced copy of kb_worker_survived_termination (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int kb_worker_survived_termination(const char *term_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kb_worker_survived_termination <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kb_worker_survived_termination");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = kb_worker_survived_termination(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kb_worker_survived_termination");
    }
    return 0;
}
