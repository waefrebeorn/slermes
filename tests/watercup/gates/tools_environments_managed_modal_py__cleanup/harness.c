/* gate: tools_environments_managed_modal_py__cleanup — traced copy of emm_cleanup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int emm_cleanup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("emm_cleanup");
    int result = emm_cleanup(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("emm_cleanup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s emm_cleanup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
