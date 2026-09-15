/* gate: hermes_cli_update_cmd_py___is_zip_preserved_entry_status_line — traced copy of _is_zip_preserved_entry_status_line */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_zip_preserved_entry_status_line(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_zip_preserved_entry_status_line");
    int result = _is_zip_preserved_entry_status_line(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_zip_preserved_entry_status_line");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_zip_preserved_entry_status_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
