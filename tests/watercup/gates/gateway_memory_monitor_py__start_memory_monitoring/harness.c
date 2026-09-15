/* gate: gateway_memory_monitor_py__start_memory_monitoring — traced copy of start_memory_monitoring */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int start_memory_monitoring(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("start_memory_monitoring");
    int result = start_memory_monitoring(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("start_memory_monitoring");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s start_memory_monitoring <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
