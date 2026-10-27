/* gate: port__hermes_cli_auth_py___persist_provider_state_to_store — traced copy of authstore_persist_provider_state_to_store (json_param) */
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


extern char * authstore_persist_provider_state_to_store(const char *provider_id, const json_t *state,
                                                const char *target_path, bool set_active);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authstore_persist_provider_state_to_store <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("authstore_persist_provider_state_to_store");
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = authstore_persist_provider_state_to_store(argv[i], _j1, argv[2 + (i - 2 + 2) % 5], (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j1) json_free(_j1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("authstore_persist_provider_state_to_store");
    }
    return 0;
}
