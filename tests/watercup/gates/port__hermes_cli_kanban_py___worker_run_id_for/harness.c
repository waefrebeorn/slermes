/* gate: port__hermes_cli_kanban_py___worker_run_id_for — traced copy of kanban_worker_run_id_for (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern long kanban_worker_run_id_for(const char *task_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kanban_worker_run_id_for <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kanban_worker_run_id_for");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = kanban_worker_run_id_for((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kanban_worker_run_id_for");
    }
    return 0;
}
