/* gate: port__hermes_cli_proxy_adapters_nous_portal_py___read_state — traced copy of cli_hermes_cli_proxy_adapters_nous_portal__read_state (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_hermes_cli_proxy_adapters_nous_portal__read_state(const char *auth_path, char *state_json, size_t state_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_proxy_adapters_nous_portal__read_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_proxy_adapters_nous_portal__read_state");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_hermes_cli_proxy_adapters_nous_portal__read_state(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_proxy_adapters_nous_portal__read_state");
    }
    return 0;
}
