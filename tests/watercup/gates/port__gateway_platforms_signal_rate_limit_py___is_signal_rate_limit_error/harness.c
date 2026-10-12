/* gate: port__gateway_platforms_signal_rate_limit_py___is_signal_rate_limit_error — traced copy of cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(const char *err);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error");
    }
    return 0;
}
