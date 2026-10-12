/* gate: port__hermes_cli_web_routers_local_models_py___variant_files_on_disk — traced copy of _variant_files_on_disk (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _variant_files_on_disk(const char *min_engine);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _variant_files_on_disk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_variant_files_on_disk");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _variant_files_on_disk(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_variant_files_on_disk");
    }
    return 0;
}
