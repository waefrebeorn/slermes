/* gate: utils_py__warn_if_credential_file_broadly_readable — traced copy of util_warn_if_credential_file_broadly_readable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool util_warn_if_credential_file_broadly_readable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("util_warn_if_credential_file_broadly_readable");
    bool result = util_warn_if_credential_file_broadly_readable(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("util_warn_if_credential_file_broadly_readable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s util_warn_if_credential_file_broadly_readable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
