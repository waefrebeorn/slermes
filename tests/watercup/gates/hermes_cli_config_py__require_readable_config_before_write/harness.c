/* gate: hermes_cli_config_py__require_readable_config_before_write — traced copy of config_py_require_readable_config_before_write */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_py_require_readable_config_before_write(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_py_require_readable_config_before_write");
    int result = config_py_require_readable_config_before_write(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_py_require_readable_config_before_write");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_py_require_readable_config_before_write <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
