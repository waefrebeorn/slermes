/* gate: real__cron_scheduler_provider_py__provider_supports_split_fire — GAP sentinel for provider_supports_split_fire */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "provider_supports_split_fire";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_supports_split_fire <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("provider_supports_split_fire");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("provider_supports_split_fire");
    }
    return 0;
}
