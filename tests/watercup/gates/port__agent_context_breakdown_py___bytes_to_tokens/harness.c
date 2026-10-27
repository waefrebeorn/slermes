/* gate: port__agent_context_breakdown_py___bytes_to_tokens — traced copy of context_breakdown_bytes_to_tokens (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long context_breakdown_bytes_to_tokens(long size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s context_breakdown_bytes_to_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("context_breakdown_bytes_to_tokens");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = context_breakdown_bytes_to_tokens((long)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("context_breakdown_bytes_to_tokens");
    }
    return 0;
}
