/* gate: port__gateway_platforms_base_py__on_processing_complete — traced copy of pbs3_on_processing_complete (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int pbs3_on_processing_complete(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pbs3_on_processing_complete <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pbs3_on_processing_complete");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = pbs3_on_processing_complete();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pbs3_on_processing_complete");
    }
    return 0;
}
