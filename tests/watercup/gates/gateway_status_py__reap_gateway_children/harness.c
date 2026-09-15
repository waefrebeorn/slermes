/* gate: gateway_status_py__reap_gateway_children — traced copy of gstat_reap_gateway_children */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gstat_reap_gateway_children(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gstat_reap_gateway_children");
    int result = gstat_reap_gateway_children(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gstat_reap_gateway_children");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gstat_reap_gateway_children <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
