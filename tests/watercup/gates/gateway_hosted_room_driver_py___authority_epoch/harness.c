/* gate: gateway_hosted_room_driver_py___authority_epoch — traced copy of _authority_epoch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _authority_epoch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_authority_epoch");
    int result = _authority_epoch(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_authority_epoch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _authority_epoch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
