/* gate: port__agent_context_compressor_py___estimate_msg_budget_tokens — traced copy of cc_micro_compact_estimate_msg_budget_tokens (json_param) */
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


extern long cc_micro_compact_estimate_msg_budget_tokens(json_t *msg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cc_micro_compact_estimate_msg_budget_tokens <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cc_micro_compact_estimate_msg_budget_tokens");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = cc_micro_compact_estimate_msg_budget_tokens(_j0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cc_micro_compact_estimate_msg_budget_tokens");
    }
    return 0;
}
