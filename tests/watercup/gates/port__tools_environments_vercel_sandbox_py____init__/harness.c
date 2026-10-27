/* gate: port__tools_environments_vercel_sandbox_py____init__ — traced copy of ve_env_init (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void ve_env_init(const char *task_id, bool persistent, double timeout_secs,
                 const char *runtime);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ve_env_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ve_env_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    ve_env_init();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ve_env_init");
    }
    return 0;
}
