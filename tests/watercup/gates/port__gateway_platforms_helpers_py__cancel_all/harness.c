/* gate: port__gateway_platforms_helpers_py__cancel_all — traced copy of helpers_cancel_all (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void helpers_cancel_all(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers_cancel_all <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("helpers_cancel_all");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    helpers_cancel_all();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("helpers_cancel_all");
    }
    return 0;
}
