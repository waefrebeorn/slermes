/* gate: port__hermes_cli_update_cmd_py___write_update_planned_stop_marker — traced copy of uc_write_update_planned_stop_marker (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int uc_write_update_planned_stop_marker(const char *profile_path, long pid,
                                        long stopper_pid,
                                        const char *target_start_time,
                                        const char *written_at);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_write_update_planned_stop_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_write_update_planned_stop_marker");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = uc_write_update_planned_stop_marker(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_write_update_planned_stop_marker");
    }
    return 0;
}
