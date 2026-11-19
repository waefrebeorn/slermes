/* gate: port__hermes_cli_observability_shared_metrics_contract_py__counter_dimensions_are_valid — traced copy of smc_counter_dimensions_are_valid (json_param) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern bool smc_counter_dimensions_are_valid(const char *metric, json_t *dims);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smc_counter_dimensions_are_valid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("smc_counter_dimensions_are_valid");
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = smc_counter_dimensions_are_valid(argv[i], _j1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j1) json_free(_j1);
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("smc_counter_dimensions_are_valid");
    }
    return 0;
}
