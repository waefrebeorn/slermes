/* gate: port__gateway_platforms_api_server_room_grants_py___room_grant_error_response — traced copy of _room_grant_error_response (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _room_grant_error_response(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _room_grant_error_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_room_grant_error_response");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _room_grant_error_response(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_room_grant_error_response");
    }
    return 0;
}
