/* gate: port__gateway_relay_ws_transport_py___media_types_from_wire — traced copy of _media_types_from_wire (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _media_types_from_wire(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _media_types_from_wire <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_media_types_from_wire");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _media_types_from_wire();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_media_types_from_wire");
    }
    return 0;
}
