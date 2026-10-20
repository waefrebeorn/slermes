/* gate: port__hermes_cli_models_py__partition_nous_models_by_tier — traced copy of partition_nous_models_by_tier_json (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * partition_nous_models_by_tier_json(const char *model_ids_json,
                                         const char *pricing_json, int free_tier);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s partition_nous_models_by_tier_json <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("partition_nous_models_by_tier_json");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = partition_nous_models_by_tier_json(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("partition_nous_models_by_tier_json");
    }
    return 0;
}
