/* gate: gateway_session_py___drain_spooled_drops — traced copy of session_drain_spooled_drops */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int session_drain_spooled_drops(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("session_drain_spooled_drops");
    int result = session_drain_spooled_drops(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("session_drain_spooled_drops");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_drain_spooled_drops <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
