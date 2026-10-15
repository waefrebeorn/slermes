/* gate: port__gateway_run_py___deep_merge_request_overrides — traced copy of _deep_merge_request_overrides (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _deep_merge_request_overrides(const char *base, const char *override);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _deep_merge_request_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_deep_merge_request_overrides");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _deep_merge_request_overrides(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_deep_merge_request_overrides");
    }
    return 0;
}
