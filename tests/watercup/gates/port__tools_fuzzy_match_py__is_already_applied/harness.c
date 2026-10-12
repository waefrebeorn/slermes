/* gate: port__tools_fuzzy_match_py__is_already_applied — traced copy of fuzzy_is_already_applied (bool_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool fuzzy_is_already_applied(const char *content, const char *old_string,
                              const char *new_string);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fuzzy_is_already_applied <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fuzzy_is_already_applied");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fuzzy_is_already_applied((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_is_already_applied");
    }
    return 0;
}
