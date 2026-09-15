/* gate: gateway_platforms_qqbot_onboard_py___render_qr — traced copy of qqbot_onboard__render_qr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int qqbot_onboard__render_qr(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("qqbot_onboard__render_qr");
    int result = qqbot_onboard__render_qr(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("qqbot_onboard__render_qr");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqbot_onboard__render_qr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
