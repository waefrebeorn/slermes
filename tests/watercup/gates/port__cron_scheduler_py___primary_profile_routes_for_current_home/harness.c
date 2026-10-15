/* gate: port__cron_scheduler_py___primary_profile_routes_for_current_home — traced copy of _primary_profile_routes_for_current_home (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _primary_profile_routes_for_current_home(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _primary_profile_routes_for_current_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_primary_profile_routes_for_current_home");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _primary_profile_routes_for_current_home();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_primary_profile_routes_for_current_home");
    }
    return 0;
}
