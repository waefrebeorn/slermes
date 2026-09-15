/* gate: gateway_platforms_yuanbao_py__group_policy — traced copy of yb_group_policy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_group_policy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_group_policy");
    int result = yb_group_policy(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_group_policy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_group_policy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
