/* gate: port__tools_browser_camofox_py___camofox_identity_override — traced copy of _camofox_identity_override (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _camofox_identity_override(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _camofox_identity_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_camofox_identity_override");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _camofox_identity_override();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_camofox_identity_override");
    }
    return 0;
}
