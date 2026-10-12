/* gate: port__hermes_cli_auth_py___config_provider_matches — traced copy of auth_config_provider_matches (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool auth_config_provider_matches(const char *provider_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_config_provider_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_config_provider_matches");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = auth_config_provider_matches(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_config_provider_matches");
    }
    return 0;
}
