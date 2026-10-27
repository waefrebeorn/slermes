/* gate: port__agent_anthropic_adapter_py___resolve_anthropic_messages_max_tokens — traced copy of antr_resolve_anthropic_messages_max_tokens (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long antr_resolve_anthropic_messages_max_tokens(long requested, const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_resolve_anthropic_messages_max_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("antr_resolve_anthropic_messages_max_tokens");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = antr_resolve_anthropic_messages_max_tokens((long)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("antr_resolve_anthropic_messages_max_tokens");
    }
    return 0;
}
