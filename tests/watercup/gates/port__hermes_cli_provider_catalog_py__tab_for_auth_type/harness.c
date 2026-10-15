/* gate: port__hermes_cli_provider_catalog_py__tab_for_auth_type — traced copy of provider_catalog_tab_for_auth_type (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * provider_catalog_tab_for_auth_type(const char *auth_type);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_catalog_tab_for_auth_type <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("provider_catalog_tab_for_auth_type");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = provider_catalog_tab_for_auth_type(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("provider_catalog_tab_for_auth_type");
    }
    return 0;
}
