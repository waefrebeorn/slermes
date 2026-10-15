/* gate: port__tools_fuzzy_match_py__format_no_match_hint — traced copy of fuzzy_match_format_no_match_hint (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fuzzy_match_format_no_match_hint(const char *error, int match_count, const char *old_string, const char *content);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fuzzy_match_format_no_match_hint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fuzzy_match_format_no_match_hint");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fuzzy_match_format_no_match_hint((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_match_format_no_match_hint");
    }
    return 0;
}
