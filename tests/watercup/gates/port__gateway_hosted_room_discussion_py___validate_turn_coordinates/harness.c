/* gate: port__gateway_hosted_room_discussion_py___validate_turn_coordinates — traced copy of _validate_turn_coordinates (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _validate_turn_coordinates(const char *payload);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _validate_turn_coordinates <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_validate_turn_coordinates");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _validate_turn_coordinates(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_validate_turn_coordinates");
    }
    return 0;
}
