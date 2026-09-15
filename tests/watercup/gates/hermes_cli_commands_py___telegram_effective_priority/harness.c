/* gate: hermes_cli_commands_py___telegram_effective_priority — traced copy of cmd_telegram_effective_priority */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cmd_telegram_effective_priority(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cmd_telegram_effective_priority");
    int result = cmd_telegram_effective_priority(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cmd_telegram_effective_priority");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_telegram_effective_priority <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
