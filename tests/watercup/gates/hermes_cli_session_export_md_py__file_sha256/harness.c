/* gate: hermes_cli_session_export_md_py__file_sha256 — traced copy of sexmd_file_sha256 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexmd_file_sha256(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexmd_file_sha256");
    int result = sexmd_file_sha256(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexmd_file_sha256");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexmd_file_sha256 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
