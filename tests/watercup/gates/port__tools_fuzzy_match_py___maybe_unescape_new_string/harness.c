/* gate: port__tools_fuzzy_match_py___maybe_unescape_new_string — traced copy of fuzzy_match_maybe_unescape_new_string (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fuzzy_match_maybe_unescape_new_string(const char *new_string, const char *content, long *matches, size_t nmatch);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fuzzy_match_maybe_unescape_new_string <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fuzzy_match_maybe_unescape_new_string");
    long _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fuzzy_match_maybe_unescape_new_string(argv[i], argv[2 + (i - 2 + 1) % 5], &_o2, (long)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%ld", (long)_o2); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_match_maybe_unescape_new_string");
    }
    return 0;
}
