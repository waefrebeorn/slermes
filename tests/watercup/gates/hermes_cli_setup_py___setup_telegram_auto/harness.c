/* gate: hermes_cli_setup_py___setup_telegram_auto — traced copy of _setup_telegram_auto */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _setup_telegram_auto(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_setup_telegram_auto");
    int result = _setup_telegram_auto(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_setup_telegram_auto");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _setup_telegram_auto <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
