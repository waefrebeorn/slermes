/* gate: port__gateway_platforms_signal_py___remux_aac_to_m4a — traced copy of _remux_aac_to_m4a (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool _remux_aac_to_m4a(const char* input_path, const char* output_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _remux_aac_to_m4a <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_remux_aac_to_m4a");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = _remux_aac_to_m4a(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_remux_aac_to_m4a");
    }
    return 0;
}
