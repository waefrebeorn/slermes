/* gate: gateway_platforms_qqbot_adapter_py__check_qq_requirements — traced copy of qqbot_check_qq_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int qqbot_check_qq_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("qqbot_check_qq_requirements");
    int result = qqbot_check_qq_requirements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("qqbot_check_qq_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqbot_check_qq_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
