/* gate: port__hermes_cli_update_cmd_py___install_psutil_android_compat — traced copy of uc_install_psutil_android_compat (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int uc_install_psutil_android_compat(const char *install_cmd_prefix[]);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_install_psutil_android_compat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_install_psutil_android_compat");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = uc_install_psutil_android_compat(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_install_psutil_android_compat");
    }
    return 0;
}
