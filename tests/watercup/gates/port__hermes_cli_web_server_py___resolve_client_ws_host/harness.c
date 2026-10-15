/* gate: port__hermes_cli_web_server_py___resolve_client_ws_host — traced copy of ws_events_resolve_client_host (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ws_events_resolve_client_host(const char *bound_host);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_events_resolve_client_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_events_resolve_client_host");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ws_events_resolve_client_host(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_events_resolve_client_host");
    }
    return 0;
}
