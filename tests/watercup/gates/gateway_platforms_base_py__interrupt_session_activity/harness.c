/* gate: gateway_platforms_base_py__interrupt_session_activity — traced copy of gw_base__interrupt_session_activity */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_base__interrupt_session_activity(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("gw_base__interrupt_session_activity");
    int result = gw_base__interrupt_session_activity(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_base__interrupt_session_activity");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_base__interrupt_session_activity <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
