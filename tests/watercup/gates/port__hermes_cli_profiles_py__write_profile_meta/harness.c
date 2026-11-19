/* gate: port__hermes_cli_profiles_py__write_profile_meta — traced copy of profile_write_profile_meta (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool profile_write_profile_meta(const char *profile_dir, const char *description,
                                bool has_description, bool desc_auto,
                                bool has_desc_auto);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_write_profile_meta <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profile_write_profile_meta");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = profile_write_profile_meta(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profile_write_profile_meta");
    }
    return 0;
}
