/* gate: gateway_status_py__normalize_updated_at — traced copy of gstat_normalize_updated_at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gstat_normalize_updated_at(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gstat_normalize_updated_at");
    int result = gstat_normalize_updated_at(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gstat_normalize_updated_at");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gstat_normalize_updated_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
