/* gate: port__hermes_cli_kanban_db_py__worker_log_rotation_config — traced copy of kdb_worker_log_rotation_config (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void kdb_worker_log_rotation_config(const char *kanban_cfg_json,
                                    long *out_rotate_bytes,
                                    int *out_backup_count);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_worker_log_rotation_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kdb_worker_log_rotation_config");
    long _o1 = 0;
    int _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    kdb_worker_log_rotation_config(argv[i], &_o1, &_o2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%ld", (long)_o1); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kdb_worker_log_rotation_config");
    }
    return 0;
}
