/* gate: port__hermes_cli_setup_py___is_valid_telegram_bot_token — traced copy of stp_is_valid_telegram_bot_token (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool stp_is_valid_telegram_bot_token(const char *token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_is_valid_telegram_bot_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_is_valid_telegram_bot_token");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = stp_is_valid_telegram_bot_token(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_is_valid_telegram_bot_token");
    }
    return 0;
}
