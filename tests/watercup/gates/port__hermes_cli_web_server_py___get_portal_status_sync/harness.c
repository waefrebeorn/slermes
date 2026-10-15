/* gate: port__hermes_cli_web_server_py___get_portal_status_sync — traced copy of ws_get_portal_status_sync (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void ws_get_portal_status_sync(char *out_json, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_get_portal_status_sync <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_get_portal_status_sync");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    ws_get_portal_status_sync();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_get_portal_status_sync");
    }
    return 0;
}
