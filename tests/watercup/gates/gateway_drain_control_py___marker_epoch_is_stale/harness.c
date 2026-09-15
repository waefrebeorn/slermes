/* gate: gateway_drain_control_py___marker_epoch_is_stale — traced copy of drain_control_marker_epoch_is_stale */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int drain_control_marker_epoch_is_stale(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("drain_control_marker_epoch_is_stale");
    int result = drain_control_marker_epoch_is_stale(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("drain_control_marker_epoch_is_stale");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s drain_control_marker_epoch_is_stale <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
