/* gate: port__hermes_cli_linux_desktop_entry_py__refresh_desktop_databases — traced copy of lde_refresh_desktop_databases (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * lde_refresh_desktop_databases(const char *applications_dir, int *out_ran);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lde_refresh_desktop_databases <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("lde_refresh_desktop_databases");
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = lde_refresh_desktop_databases(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("lde_refresh_desktop_databases");
    }
    return 0;
}
