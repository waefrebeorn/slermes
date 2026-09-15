/* gate: hermes_cli_config_py___phantom_sibling — traced copy of _phantom_sibling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _phantom_sibling(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_phantom_sibling");
    int result = _phantom_sibling(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_phantom_sibling");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _phantom_sibling <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
