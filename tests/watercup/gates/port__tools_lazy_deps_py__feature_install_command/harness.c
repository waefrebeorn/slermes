/* gate: port__tools_lazy_deps_py__feature_install_command — traced copy of lazy_feature_install_command (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * lazy_feature_install_command(const char *feature);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lazy_feature_install_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("lazy_feature_install_command");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = lazy_feature_install_command(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("lazy_feature_install_command");
    }
    return 0;
}
