/* gate: port__tools_fuzzy_match_py___detect_escape_drift — traced copy of _detect_escape_drift (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _detect_escape_drift(const char *content, size_t ms, size_t ml,
                                const char *old_string, const char *new_string, char *err, size_t errsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _detect_escape_drift <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_detect_escape_drift");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _detect_escape_drift(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], (long)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_detect_escape_drift");
    }
    return 0;
}
