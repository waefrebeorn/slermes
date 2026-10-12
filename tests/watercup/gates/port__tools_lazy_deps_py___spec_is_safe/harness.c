/* gate: port__tools_lazy_deps_py___spec_is_safe — traced copy of lazy_deps_spec_is_safe (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int lazy_deps_spec_is_safe(const char *spec);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lazy_deps_spec_is_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("lazy_deps_spec_is_safe");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = lazy_deps_spec_is_safe(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("lazy_deps_spec_is_safe");
    }
    return 0;
}
