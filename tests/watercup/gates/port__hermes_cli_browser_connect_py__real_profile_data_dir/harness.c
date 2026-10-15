/* gate: port__hermes_cli_browser_connect_py__real_profile_data_dir — traced copy of real_profile_data_dir (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * real_profile_data_dir(const char *browser, const char *system);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s real_profile_data_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("real_profile_data_dir");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = real_profile_data_dir(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("real_profile_data_dir");
    }
    return 0;
}
