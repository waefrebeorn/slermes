/* gate: port__hermes_cli_auth_py__write_credential_pool — traced copy of write_credential_pool (json_param) */
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


extern char * write_credential_pool(const char *provider_id, const json_t *entries,
                            const json_t *removed_ids);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s write_credential_pool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("write_credential_pool");
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    json_t *_j2 = json_parse(argv[2 + (i - 2 + 2) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = write_credential_pool(argv[i], _j1, _j2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j1) json_free(_j1);
    if (_j2) json_free(_j2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("write_credential_pool");
    }
    return 0;
}
