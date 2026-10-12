/* gate: port__tools_file_operations_py___looks_like_linter_unusable — traced copy of fo_looks_like_linter_unusable (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int fo_looks_like_linter_unusable(const char *base_cmd, const char *output);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_looks_like_linter_unusable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_looks_like_linter_unusable");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = fo_looks_like_linter_unusable(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_looks_like_linter_unusable");
    }
    return 0;
}
