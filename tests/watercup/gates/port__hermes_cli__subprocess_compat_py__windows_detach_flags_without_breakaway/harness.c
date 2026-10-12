/* gate: port__hermes_cli__subprocess_compat_py__windows_detach_flags_without_breakaway — traced copy of hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay");
    }
    return 0;
}
