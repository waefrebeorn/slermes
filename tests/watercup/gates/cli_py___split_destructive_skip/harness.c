/* gate: cli_py___split_destructive_skip — traced copy of hcli_split_destructive_skip */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hcli_split_destructive_skip(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hcli_split_destructive_skip");
    int result = hcli_split_destructive_skip(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hcli_split_destructive_skip");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_split_destructive_skip <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
