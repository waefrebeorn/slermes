/* gate: port__hermes_cli_approvals_suggest_py__apply_proposals — traced copy of asuggest_apply_proposals (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int asuggest_apply_proposals(proposal_t **props, size_t n, const int *indices, int n_idx,
                             char ***out_merged, size_t *out_n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s asuggest_apply_proposals <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("asuggest_apply_proposals");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = asuggest_apply_proposals(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("asuggest_apply_proposals");
    }
    return 0;
}
