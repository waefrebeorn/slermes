/* gate: port__agent_retry_utils_py__parse_retry_after_seconds — traced copy of retry_utils_parse_retry_after_seconds (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double retry_utils_parse_retry_after_seconds(const char *value, int *ok);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s retry_utils_parse_retry_after_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("retry_utils_parse_retry_after_seconds");
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = retry_utils_parse_retry_after_seconds(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("retry_utils_parse_retry_after_seconds");
    }
    return 0;
}
