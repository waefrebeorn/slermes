/* gate: port__hermes_cli_update_inventory_py___detect_supervisor_for_pid — traced copy of update_inventory_detect_supervisor_for_pid (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void update_inventory_detect_supervisor_for_pid(long pid,
                                                const long *service_pids,
                                                size_t n, char out[16]);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s update_inventory_detect_supervisor_for_pid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("update_inventory_detect_supervisor_for_pid");
    long _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    update_inventory_detect_supervisor_for_pid((long)strtol(argv[i], NULL, 10), &_o1, (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%ld", (long)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("update_inventory_detect_supervisor_for_pid");
    }
    return 0;
}
