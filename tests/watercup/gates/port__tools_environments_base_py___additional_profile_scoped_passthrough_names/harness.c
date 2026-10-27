/* gate: port__tools_environments_base_py___additional_profile_scoped_passthrough_names — traced copy of envb_additional_profile_scoped_passthrough_names (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** envb_additional_profile_scoped_passthrough_names(int *out_count);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_additional_profile_scoped_passthrough_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("envb_additional_profile_scoped_passthrough_names");
    int _o0 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = envb_additional_profile_scoped_passthrough_names(&_o0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("envb_additional_profile_scoped_passthrough_names");
    }
    return 0;
}
