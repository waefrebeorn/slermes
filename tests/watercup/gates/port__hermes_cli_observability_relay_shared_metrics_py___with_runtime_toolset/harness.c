/* gate: port__hermes_cli_observability_relay_shared_metrics_py___with_runtime_toolset — traced copy of rsm_with_runtime_toolset (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * rsm_with_runtime_toolset(const char *tool_name, const char *event_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rsm_with_runtime_toolset <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rsm_with_runtime_toolset");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = rsm_with_runtime_toolset(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rsm_with_runtime_toolset");
    }
    return 0;
}
