/* gate: tools_process_registry_py___stop_systemd_unit — traced copy of process_registry_stop_systemd_unit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool process_registry_stop_systemd_unit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("process_registry_stop_systemd_unit");
    bool result = process_registry_stop_systemd_unit(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("process_registry_stop_systemd_unit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s process_registry_stop_systemd_unit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
