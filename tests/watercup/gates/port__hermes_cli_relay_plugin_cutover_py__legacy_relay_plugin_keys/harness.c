/* gate: port__hermes_cli_relay_plugin_cutover_py__legacy_relay_plugin_keys — traced copy of cutover_legacy_relay_plugin_keys (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** cutover_legacy_relay_plugin_keys(const char *const *values, size_t n,
                                        size_t *out_n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cutover_legacy_relay_plugin_keys <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cutover_legacy_relay_plugin_keys");
    size_t _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = cutover_legacy_relay_plugin_keys(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), &_o2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o2); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cutover_legacy_relay_plugin_keys");
    }
    return 0;
}
