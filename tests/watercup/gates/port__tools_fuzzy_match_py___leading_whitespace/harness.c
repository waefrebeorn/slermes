/* gate: port__tools_fuzzy_match_py___leading_whitespace — traced copy of fuzzy_match_leading_whitespace (sizet_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern size_t fuzzy_match_leading_whitespace(const char *line);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fuzzy_match_leading_whitespace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fuzzy_match_leading_whitespace");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    size_t result = fuzzy_match_leading_whitespace((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_match_leading_whitespace");
    }
    return 0;
}
