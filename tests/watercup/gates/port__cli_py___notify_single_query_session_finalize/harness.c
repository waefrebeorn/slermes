/* gate: port__cli_py___notify_single_query_session_finalize — traced copy of _notify_single_query_session_finalize (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _notify_single_query_session_finalize(const char *cli_session_id,
                                           const char *agent_session_id,
                                           const char *agent_platform,
                                           const char *reason);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _notify_single_query_session_finalize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_notify_single_query_session_finalize");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _notify_single_query_session_finalize(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_notify_single_query_session_finalize");
    }
    return 0;
}
