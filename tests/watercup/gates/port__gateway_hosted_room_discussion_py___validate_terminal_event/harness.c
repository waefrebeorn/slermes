/* gate: port__gateway_hosted_room_discussion_py___validate_terminal_event — traced copy of _validate_terminal_event (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _validate_terminal_event(const char *kind, const char *payload);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _validate_terminal_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_validate_terminal_event");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _validate_terminal_event(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_validate_terminal_event");
    }
    return 0;
}
