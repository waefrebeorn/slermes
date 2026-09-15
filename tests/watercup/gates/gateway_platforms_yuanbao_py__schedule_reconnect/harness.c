/* gate: gateway_platforms_yuanbao_py__schedule_reconnect — traced copy of yb_schedule_reconnect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_schedule_reconnect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_schedule_reconnect");
    int result = yb_schedule_reconnect(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_schedule_reconnect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_schedule_reconnect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
