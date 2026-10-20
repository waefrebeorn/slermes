/* gate: port__tools_fuzzy_match_py___preserve_unicode_in_replacement — traced copy of _preserve_unicode_in_replacement (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _preserve_unicode_in_replacement(const char *file_region,
                                               const char *old_string,
                                               const char *new_string);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _preserve_unicode_in_replacement <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_preserve_unicode_in_replacement");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _preserve_unicode_in_replacement(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_preserve_unicode_in_replacement");
    }
    return 0;
}
