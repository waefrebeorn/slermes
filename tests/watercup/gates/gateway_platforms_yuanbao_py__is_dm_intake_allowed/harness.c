/* gate: gateway_platforms_yuanbao_py__is_dm_intake_allowed — traced copy of yb_is_dm_intake_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_is_dm_intake_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_is_dm_intake_allowed");
    int result = yb_is_dm_intake_allowed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_is_dm_intake_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_is_dm_intake_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
