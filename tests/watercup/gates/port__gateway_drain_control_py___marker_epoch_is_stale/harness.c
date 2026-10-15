/* gate: port__gateway_drain_control_py___marker_epoch_is_stale — traced copy of drain_control_marker_epoch_is_stale (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int drain_control_marker_epoch_is_stale(const char *body_json, const char *current_epoch);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s drain_control_marker_epoch_is_stale <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("drain_control_marker_epoch_is_stale");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = drain_control_marker_epoch_is_stale(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("drain_control_marker_epoch_is_stale");
    }
    return 0;
}
