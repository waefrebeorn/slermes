/* gate: port__hermes_cli_update_cmd_py___stop_process_trees — traced copy of update_stop_process_trees (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int update_stop_process_trees(const char *pid_list);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s update_stop_process_trees <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("update_stop_process_trees");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = update_stop_process_trees(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("update_stop_process_trees");
    }
    return 0;
}
