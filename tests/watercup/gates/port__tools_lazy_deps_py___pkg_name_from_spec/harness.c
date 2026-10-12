/* gate: port__tools_lazy_deps_py___pkg_name_from_spec — traced copy of lazy_pkg_name_from_spec (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void lazy_pkg_name_from_spec(const char *spec, char *out, size_t out_cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lazy_pkg_name_from_spec <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("lazy_pkg_name_from_spec");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    lazy_pkg_name_from_spec(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("lazy_pkg_name_from_spec");
    }
    return 0;
}
