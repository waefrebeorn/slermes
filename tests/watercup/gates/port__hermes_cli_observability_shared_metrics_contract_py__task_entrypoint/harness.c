/* gate: port__hermes_cli_observability_shared_metrics_contract_py__task_entrypoint — traced copy of smc_task_entrypoint (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * smc_task_entrypoint(const char *entrypoint_val,
                                 const char *surface,
                                 bool has_parent_task,
                                 bool has_parent_session);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_task_entrypoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_task_entrypoint");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = smc_task_entrypoint((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_task_entrypoint");
    }
    return 0;
}
