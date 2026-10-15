/* gate: port__hermes_cli_uninstall_py__remove_windows_bin_launchers — traced copy of remove_windows_bin_launchers (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * remove_windows_bin_launchers(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s remove_windows_bin_launchers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("remove_windows_bin_launchers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = remove_windows_bin_launchers();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("remove_windows_bin_launchers");
    }
    return 0;
}
