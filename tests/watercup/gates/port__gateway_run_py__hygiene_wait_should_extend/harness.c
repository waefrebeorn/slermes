/* gate: port__gateway_run_py__hygiene_wait_should_extend — traced copy of hygiene_wait_should_extend (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int hygiene_wait_should_extend(float idle, float timeout, float waited, float ceiling, int fence_cancelled);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hygiene_wait_should_extend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hygiene_wait_should_extend");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = hygiene_wait_should_extend(strtod(argv[i], NULL), strtod(argv[2 + (i - 2 + 1) % 5], NULL), strtod(argv[2 + (i - 2 + 2) % 5], NULL), strtod(argv[2 + (i - 2 + 3) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hygiene_wait_should_extend");
    }
    return 0;
}
