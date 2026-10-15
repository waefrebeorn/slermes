/* gate: port__hermes_cli_update_cmd_py___detect_self_loaded_native_modules — traced copy of _detect_self_loaded_native_modules (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _detect_self_loaded_native_modules(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _detect_self_loaded_native_modules <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_detect_self_loaded_native_modules");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _detect_self_loaded_native_modules();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_detect_self_loaded_native_modules");
    }
    return 0;
}
