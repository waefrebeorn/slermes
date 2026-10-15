/* gate: port__agent_context_compressor_py___find_tail_cut_by_tokens — traced copy of cli_agent_context_compressor__find_tail_cut_by_tokens (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_agent_context_compressor__find_tail_cut_by_tokens(const int *token_counts, int num_messages, int budget_tokens, int summary_tokens);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_context_compressor__find_tail_cut_by_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_context_compressor__find_tail_cut_by_tokens");
    int _o0 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_agent_context_compressor__find_tail_cut_by_tokens(&_o0, (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_context_compressor__find_tail_cut_by_tokens");
    }
    return 0;
}
