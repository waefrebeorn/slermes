/* gate: port__hermes_cli_proxy_adapters_nous_portal_py__is_authenticated — traced copy of cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated(const char *auth_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated");
    }
    return 0;
}
