/* gate: port__hermes_cli_setup_py___set_credential_pool_strategy — traced copy of stp_set_credential_pool_strategy (json_param) */
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


extern void stp_set_credential_pool_strategy(json_t *config,
                                        const char *provider,
                                        const char *strategy);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_set_credential_pool_strategy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_set_credential_pool_strategy");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = stp_set_credential_pool_strategy(_j0, argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_set_credential_pool_strategy");
    }
    return 0;
}
