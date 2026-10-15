/* gate: port__cli_py___luminance_from_hex — traced copy of cli_luminance_from_hex (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool cli_luminance_from_hex(const char *hex_str, double *out_luma);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_luminance_from_hex <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_luminance_from_hex");
    double _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = cli_luminance_from_hex(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[64]; snprintf(_b, sizeof(_b), "%.17g", (double)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_luminance_from_hex");
    }
    return 0;
}
