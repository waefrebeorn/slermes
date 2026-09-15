/* gate: hermes_cli_update_cmd_py___called_process_error_is_python_dep_install — traced copy of upd_called_process_error_is_python_dep_install */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool upd_called_process_error_is_python_dep_install(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("upd_called_process_error_is_python_dep_install");
    bool result = upd_called_process_error_is_python_dep_install(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("upd_called_process_error_is_python_dep_install");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_called_process_error_is_python_dep_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
