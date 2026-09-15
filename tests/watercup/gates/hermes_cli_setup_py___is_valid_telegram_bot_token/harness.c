/* gate: hermes_cli_setup_py___is_valid_telegram_bot_token — traced copy of stp_is_valid_telegram_bot_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool stp_is_valid_telegram_bot_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("stp_is_valid_telegram_bot_token");
    bool result = stp_is_valid_telegram_bot_token(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stp_is_valid_telegram_bot_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_is_valid_telegram_bot_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
