/* gate: real__gateway_relay_adapter_py___card_key — GAP sentinel for _card_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_card_key";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _card_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_card_key");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_card_key");
    }
    return 0;
}
