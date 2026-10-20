/* gate: port__hermes_cli_model_cost_guard_py___pricing_from_model_info — traced copy of model_cost_guard_pricing_from_model_info (outparam) */
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


extern int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                              double *in_out, double *out_out,
                                              char *source, size_t source_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_cost_guard_pricing_from_model_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_cost_guard_pricing_from_model_info");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = model_cost_guard_pricing_from_model_info(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_cost_guard_pricing_from_model_info");
    }
    return 0;
}
