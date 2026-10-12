/* gate: real__hermes_cli_web_server_py__list_managed_files — GAP sentinel for list_managed_files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "list_managed_files";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s list_managed_files <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("list_managed_files");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("list_managed_files");
    }
    return 0;
}
