/* gate: port__hermes_cli_auth_py___save_provider_state_to_source — traced copy of authstore_save_provider_state_to_source (json_param) */
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


extern void authstore_save_provider_state_to_source(json_t *auth_store, const char *provider_id,
                                             const json_t *state, const char *source_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authstore_save_provider_state_to_source <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("authstore_save_provider_state_to_source");
    json_t *_j0 = json_parse(argv[i], NULL);
    json_t *_j2 = json_parse(argv[2 + (i - 2 + 2) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = authstore_save_provider_state_to_source(_j0, argv[2 + (i - 2 + 1) % 5], _j2, argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    if (_j2) json_free(_j2);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("authstore_save_provider_state_to_source");
    }
    return 0;
}
