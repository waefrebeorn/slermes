/* gate: port__gateway_authz_mixin_py___get_unauthorized_dm_behavior — traced copy of authz_get_unauthorized_dm_behavior (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * authz_get_unauthorized_dm_behavior(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authz_get_unauthorized_dm_behavior <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("authz_get_unauthorized_dm_behavior");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = authz_get_unauthorized_dm_behavior();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("authz_get_unauthorized_dm_behavior");
    }
    return 0;
}
