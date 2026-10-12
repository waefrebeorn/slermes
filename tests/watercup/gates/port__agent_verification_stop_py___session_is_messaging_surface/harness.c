/* gate: port__agent_verification_stop_py___session_is_messaging_surface — traced copy of verification_stop_session_is_messaging_surface (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int verification_stop_session_is_messaging_surface(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verification_stop_session_is_messaging_surface <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("verification_stop_session_is_messaging_surface");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = verification_stop_session_is_messaging_surface();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("verification_stop_session_is_messaging_surface");
    }
    return 0;
}
