/* gate: cron_scheduler_py___confirm_adapter_delivery — traced copy of crs_confirm_adapter_delivery */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool crs_confirm_adapter_delivery(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crs_confirm_adapter_delivery");
    bool result = crs_confirm_adapter_delivery(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crs_confirm_adapter_delivery");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crs_confirm_adapter_delivery <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
