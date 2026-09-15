/* gate: acp_adapter_entry_py__main — traced copy of acpe_main */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int acpe_main(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("acpe_main");
    int result = acpe_main(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("acpe_main");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s acpe_main <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
