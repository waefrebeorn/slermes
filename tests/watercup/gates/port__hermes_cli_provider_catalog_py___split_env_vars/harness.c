/* gate: port__hermes_cli_provider_catalog_py___split_env_vars — traced copy of provider_catalog_split_env_vars (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void provider_catalog_split_env_vars(const char *const *env_vars, int n,
                                      const char *out_keys[], int *out_keys_len,
                                      char *out_base, size_t out_base_cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_catalog_split_env_vars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("provider_catalog_split_env_vars");
    int _o3 = 0;
    char _s4[256]; _s4[0] = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    provider_catalog_split_env_vars(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5], &_o3, _s4, sizeof(_s4));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
    WCUP_VALSTR(_s4);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("provider_catalog_split_env_vars");
    }
    return 0;
}
