/* gate: hermes_cli_main_py___pytest_owns_live_checkout — traced copy of main_g_pytest_owns_live_checkout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool main_g_pytest_owns_live_checkout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_g_pytest_owns_live_checkout");
    bool result = main_g_pytest_owns_live_checkout(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_pytest_owns_live_checkout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_pytest_owns_live_checkout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
