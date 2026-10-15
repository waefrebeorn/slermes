/* gate: port__hermes_cli_web_server_py___build_sidecar_url — traced copy of ws_events_build_sidecar_url (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ws_events_build_sidecar_url(const char *bound_host, int bound_port,
                                  bool auth_required,
                                  const char *session_token,
                                  const char *internal_credential,
                                  const char *channel);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_events_build_sidecar_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_events_build_sidecar_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ws_events_build_sidecar_url((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_events_build_sidecar_url");
    }
    return 0;
}
