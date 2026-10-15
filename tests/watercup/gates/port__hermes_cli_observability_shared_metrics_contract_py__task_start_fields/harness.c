/* gate: port__hermes_cli_observability_shared_metrics_contract_py__task_start_fields — traced copy of smc_task_start_fields (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * smc_task_start_fields(const char *entrypoint_val,
                            const char *platform_val);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_task_start_fields <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_task_start_fields");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = smc_task_start_fields(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_task_start_fields");
    }
    return 0;
}
