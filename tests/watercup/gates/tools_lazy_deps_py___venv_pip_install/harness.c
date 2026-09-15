/* gate: tools_lazy_deps_py___venv_pip_install — traced copy of tools_lazy_deps_u_venv_pip_install */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_lazy_deps_u_venv_pip_install(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_lazy_deps_u_venv_pip_install");
    int result = tools_lazy_deps_u_venv_pip_install(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_lazy_deps_u_venv_pip_install");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_lazy_deps_u_venv_pip_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
