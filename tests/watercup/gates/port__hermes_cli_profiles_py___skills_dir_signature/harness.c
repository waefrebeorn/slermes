/* gate: port__hermes_cli_profiles_py___skills_dir_signature — traced copy of profile_skills_dir_signature (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double profile_skills_dir_signature(const char *skills_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_skills_dir_signature <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profile_skills_dir_signature");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = profile_skills_dir_signature(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profile_skills_dir_signature");
    }
    return 0;
}
