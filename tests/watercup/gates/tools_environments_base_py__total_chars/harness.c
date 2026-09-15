/* gate: tools_environments_base_py__total_chars — traced copy of envb_total_chars */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envb_total_chars(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envb_total_chars");
    int result = envb_total_chars(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envb_total_chars");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_total_chars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
