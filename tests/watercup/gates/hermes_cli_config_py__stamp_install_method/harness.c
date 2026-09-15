/* gate: hermes_cli_config_py__stamp_install_method — traced copy of config_stamp_install_method */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_stamp_install_method(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_stamp_install_method");
    int result = config_stamp_install_method(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_stamp_install_method");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_stamp_install_method <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
