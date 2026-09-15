/* gate: gateway_run_py___schedule_update_notification_watch — traced copy of grun_u_schedule_update_notification_watch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_schedule_update_notification_watch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_schedule_update_notification_watch");
    int result = grun_u_schedule_update_notification_watch(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_schedule_update_notification_watch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_schedule_update_notification_watch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
