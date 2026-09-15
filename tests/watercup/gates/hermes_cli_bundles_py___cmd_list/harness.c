/* gate: hermes_cli_bundles_py___cmd_list — traced copy of hcli_bundles_cmd_list */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hcli_bundles_cmd_list(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hcli_bundles_cmd_list");
    int result = hcli_bundles_cmd_list(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hcli_bundles_cmd_list");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_bundles_cmd_list <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
