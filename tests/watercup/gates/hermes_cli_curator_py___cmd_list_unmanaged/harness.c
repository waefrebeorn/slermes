/* gate: hermes_cli_curator_py___cmd_list_unmanaged — traced copy of curator_cmd_list_unmanaged */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int curator_cmd_list_unmanaged(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("curator_cmd_list_unmanaged");
    int result = curator_cmd_list_unmanaged(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("curator_cmd_list_unmanaged");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curator_cmd_list_unmanaged <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
