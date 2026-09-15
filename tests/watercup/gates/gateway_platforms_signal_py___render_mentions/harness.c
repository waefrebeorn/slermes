/* gate: gateway_platforms_signal_py___render_mentions — traced copy of gateway_platforms_signal_u_render_mentions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platforms_signal_u_render_mentions(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platforms_signal_u_render_mentions");
    int result = gateway_platforms_signal_u_render_mentions(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platforms_signal_u_render_mentions");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platforms_signal_u_render_mentions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
