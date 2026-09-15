/* gate: cron_scheduler_py___target_mirror_eligible — traced copy of _target_mirror_eligible */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _target_mirror_eligible(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_target_mirror_eligible");
    int result = _target_mirror_eligible(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_target_mirror_eligible");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _target_mirror_eligible <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
