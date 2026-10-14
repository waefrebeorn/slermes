/* gate: port__hermes_cli_plugins_py__profile_name — traced copy of profiles_normalize_profile_name (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int profiles_normalize_profile_name(const char *name, char *out, size_t outsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profiles_normalize_profile_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profiles_normalize_profile_name");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = profiles_normalize_profile_name(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profiles_normalize_profile_name");
    }
    return 0;
}
