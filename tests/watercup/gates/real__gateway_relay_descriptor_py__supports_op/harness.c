/* gate: real__gateway_relay_descriptor_py__supports_op — GAP sentinel for supports_op */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "supports_op";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s supports_op <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("supports_op");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("supports_op");
    }
    return 0;
}
