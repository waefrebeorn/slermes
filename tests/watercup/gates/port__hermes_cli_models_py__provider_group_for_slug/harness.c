/* gate: port__hermes_cli_models_py__provider_group_for_slug — traced copy of provider_group_for_slug (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * provider_group_for_slug(const char *slug);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_group_for_slug <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("provider_group_for_slug");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = provider_group_for_slug(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("provider_group_for_slug");
    }
    return 0;
}
