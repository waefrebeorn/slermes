/* gate: gateway_platforms_yuanbao_py__strip_cron_wrapper — traced copy of yb_strip_cron_wrapper */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_strip_cron_wrapper(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_strip_cron_wrapper");
    int result = yb_strip_cron_wrapper(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_strip_cron_wrapper");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_strip_cron_wrapper <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
