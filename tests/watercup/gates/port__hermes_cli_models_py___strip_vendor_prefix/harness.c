/* gate: port__hermes_cli_models_py___strip_vendor_prefix — traced copy of model_strip_vendor_prefix (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void model_strip_vendor_prefix(const char *model_id, char *out, size_t outsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_strip_vendor_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_strip_vendor_prefix");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    model_strip_vendor_prefix(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_strip_vendor_prefix");
    }
    return 0;
}
