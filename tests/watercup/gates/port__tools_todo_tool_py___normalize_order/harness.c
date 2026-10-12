/* gate: port__tools_todo_tool_py___normalize_order — traced copy of tdt_normalize_order (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void tdt_normalize_order(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tdt_normalize_order <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tdt_normalize_order");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    tdt_normalize_order(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tdt_normalize_order");
    }
    return 0;
}
