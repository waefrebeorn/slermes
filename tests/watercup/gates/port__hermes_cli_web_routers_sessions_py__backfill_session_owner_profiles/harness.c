/* gate: port__hermes_cli_web_routers_sessions_py__backfill_session_owner_profiles — traced copy of backfill_session_owner_profiles (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * backfill_session_owner_profiles(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backfill_session_owner_profiles <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("backfill_session_owner_profiles");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = backfill_session_owner_profiles();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("backfill_session_owner_profiles");
    }
    return 0;
}
