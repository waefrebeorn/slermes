/* gate: gateway_platforms_yuanbao_py__start_slow_notifier — traced copy of yb_start_slow_notifier */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_start_slow_notifier(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_start_slow_notifier");
    int result = yb_start_slow_notifier(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_start_slow_notifier");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_start_slow_notifier <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
