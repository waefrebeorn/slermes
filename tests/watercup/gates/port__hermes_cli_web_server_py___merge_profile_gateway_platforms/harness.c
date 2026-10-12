/* gate: port__hermes_cli_web_server_py___merge_profile_gateway_platforms — traced copy of _merge_profile_gateway_platforms (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _merge_profile_gateway_platforms(const char *profile);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _merge_profile_gateway_platforms <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_merge_profile_gateway_platforms");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _merge_profile_gateway_platforms(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_merge_profile_gateway_platforms");
    }
    return 0;
}
