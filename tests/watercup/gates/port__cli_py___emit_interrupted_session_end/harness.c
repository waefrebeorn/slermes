/* gate: port__cli_py___emit_interrupted_session_end — traced copy of _emit_interrupted_session_end (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _emit_interrupted_session_end(const char *cli_session_id,
                                   const char *agent_session_id,
                                   const char *task_id,
                                   const char *turn_id,
                                   const char *api_request_id,
                                   const char *model,
                                   const char *platform,
                                   const char *reason);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _emit_interrupted_session_end <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_emit_interrupted_session_end");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _emit_interrupted_session_end(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], argv[2 + (i - 2 + 6) % 5], argv[2 + (i - 2 + 7) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_emit_interrupted_session_end");
    }
    return 0;
}
