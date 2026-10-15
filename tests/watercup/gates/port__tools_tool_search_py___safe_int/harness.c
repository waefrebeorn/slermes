/* gate: port__tools_tool_search_py___safe_int — traced copy of ts_safe_int (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ts_safe_int(const char *value, long *out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_safe_int <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ts_safe_int");
    long _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ts_safe_int(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%ld", (long)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ts_safe_int");
    }
    return 0;
}
