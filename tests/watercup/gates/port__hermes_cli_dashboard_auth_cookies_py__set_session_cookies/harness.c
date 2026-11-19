/* gate: port__hermes_cli_dashboard_auth_cookies_py__set_session_cookies — traced copy of out (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int out(char *out[], int out_cap,
                              const char *access_token,
                              const char *refresh_token,
                              int access_token_expires_in,
                              int use_https, const char *prefix);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s out <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("out");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = out(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), argv[2 + (i - 2 + 6) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("out");
    }
    return 0;
}
