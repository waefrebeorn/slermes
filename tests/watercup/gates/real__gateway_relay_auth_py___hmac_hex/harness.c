/* gate: real__gateway_relay_auth_py___hmac_hex — GAP sentinel for _hmac_hex */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_hmac_hex";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _hmac_hex <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_hmac_hex");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_hmac_hex");
    }
    return 0;
}
