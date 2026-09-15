/* gate: tools_computer_use_cua_backend_py___cua_telemetry_disabled — traced copy of cua_u_cua_telemetry_disabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_u_cua_telemetry_disabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_u_cua_telemetry_disabled");
    int result = cua_u_cua_telemetry_disabled(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_u_cua_telemetry_disabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_u_cua_telemetry_disabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
