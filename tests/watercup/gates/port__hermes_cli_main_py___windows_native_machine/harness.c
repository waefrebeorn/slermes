/* gate: port__hermes_cli_main_py___windows_native_machine — traced copy of main_g_windows_native_machine (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * main_g_windows_native_machine(const char *env_arch, const char *env_archw6432);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_windows_native_machine <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("main_g_windows_native_machine");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = main_g_windows_native_machine(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("main_g_windows_native_machine");
    }
    return 0;
}
