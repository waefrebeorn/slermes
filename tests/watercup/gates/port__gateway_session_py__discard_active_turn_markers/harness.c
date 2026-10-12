/* gate: port__gateway_session_py__discard_active_turn_markers — traced copy of session_discard_active_turn_markers (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int session_discard_active_turn_markers(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_discard_active_turn_markers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("session_discard_active_turn_markers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = session_discard_active_turn_markers();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("session_discard_active_turn_markers");
    }
    return 0;
}
