/* gate: port__hermes_cli_memory_setup_py___provider_pip_dependencies — traced copy of cli_hermes_cli_memory_setup__provider_pip_dependencies (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * cli_hermes_cli_memory_setup__provider_pip_dependencies(const char *provider_name, const char *declared_deps_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_memory_setup__provider_pip_dependencies <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_memory_setup__provider_pip_dependencies");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = cli_hermes_cli_memory_setup__provider_pip_dependencies(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_memory_setup__provider_pip_dependencies");
    }
    return 0;
}
