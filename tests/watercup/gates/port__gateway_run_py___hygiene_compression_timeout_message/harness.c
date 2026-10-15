/* gate: port__gateway_run_py___hygiene_compression_timeout_message — traced copy of _hygiene_compression_timeout_message (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _hygiene_compression_timeout_message(int total_exhausted, float elapsed, float idle_timeout, int progress_observed);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _hygiene_compression_timeout_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_hygiene_compression_timeout_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _hygiene_compression_timeout_message((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL), strtod(argv[2 + (i - 2 + 2) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_hygiene_compression_timeout_message");
    }
    return 0;
}
