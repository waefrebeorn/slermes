/* gate: real__gateway_relay_ws_transport_py__go_dormant — GAP sentinel for go_dormant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "go_dormant";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s go_dormant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("go_dormant");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("go_dormant");
    }
    return 0;
}
