/* gate: port__agent_runtime_cwd_py__clear_session_cwd — traced copy of rcw_clear_session_cwd (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int rcw_clear_session_cwd(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rcw_clear_session_cwd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rcw_clear_session_cwd");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = rcw_clear_session_cwd();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rcw_clear_session_cwd");
    }
    return 0;
}
