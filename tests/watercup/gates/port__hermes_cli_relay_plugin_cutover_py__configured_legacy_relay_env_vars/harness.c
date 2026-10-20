/* gate: port__hermes_cli_relay_plugin_cutover_py__configured_legacy_relay_env_vars — traced copy of cutover_configured_legacy_relay_env_vars (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** cutover_configured_legacy_relay_env_vars(const char *const *env_names, const char *const *env_vals, size_t n,
    size_t *out_n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cutover_configured_legacy_relay_env_vars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cutover_configured_legacy_relay_env_vars");
    size_t _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = cutover_configured_legacy_relay_env_vars(argv[i], argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cutover_configured_legacy_relay_env_vars");
    }
    return 0;
}
