/* gate: tools_skill_usage_py__seed_record_if_missing — traced copy of su_seed_record_if_missing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_seed_record_if_missing(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("su_seed_record_if_missing");
    int result = su_seed_record_if_missing(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_seed_record_if_missing");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_seed_record_if_missing <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
