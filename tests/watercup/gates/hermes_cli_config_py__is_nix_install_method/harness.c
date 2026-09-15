/* gate: hermes_cli_config_py__is_nix_install_method — traced copy of is_nix_install_method */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_nix_install_method(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_nix_install_method");
    bool result = is_nix_install_method(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_nix_install_method");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_nix_install_method <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
