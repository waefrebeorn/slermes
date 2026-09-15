/* gate: tools_async_delegation_py__get_durable_delegation — traced copy of adel_get_durable_delegation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_get_durable_delegation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_get_durable_delegation");
    int result = adel_get_durable_delegation(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_get_durable_delegation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_get_durable_delegation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
