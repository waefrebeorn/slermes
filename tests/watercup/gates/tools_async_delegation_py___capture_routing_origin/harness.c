/* gate: tools_async_delegation_py___capture_routing_origin — traced copy of ade_capture_routing_origin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ade_capture_routing_origin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ade_capture_routing_origin");
    int result = ade_capture_routing_origin(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ade_capture_routing_origin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ade_capture_routing_origin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
