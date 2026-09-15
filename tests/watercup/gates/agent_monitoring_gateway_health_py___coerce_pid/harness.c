/* gate: agent_monitoring_gateway_health_py___coerce_pid — traced copy of str_in_list */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int str_in_list(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("str_in_list");
    int result = str_in_list(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("str_in_list");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s str_in_list <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
