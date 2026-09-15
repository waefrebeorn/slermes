/* gate: cron_lifecycle_guard_py___is_cloud_placeholder_path — traced copy of cron_lifecycle_is_cloud_placeholder_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cron_lifecycle_is_cloud_placeholder_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_lifecycle_is_cloud_placeholder_path");
    bool result = cron_lifecycle_is_cloud_placeholder_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_lifecycle_is_cloud_placeholder_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_lifecycle_is_cloud_placeholder_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
