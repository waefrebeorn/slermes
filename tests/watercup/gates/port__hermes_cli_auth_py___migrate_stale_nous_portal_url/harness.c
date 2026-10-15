/* gate: port__hermes_cli_auth_py___migrate_stale_nous_portal_url — traced copy of auth_migrate_stale_nous_portal_url (json_param) */
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


extern void auth_migrate_stale_nous_portal_url(json_t *providers);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_migrate_stale_nous_portal_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_migrate_stale_nous_portal_url");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = auth_migrate_stale_nous_portal_url(_j0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_migrate_stale_nous_portal_url");
    }
    return 0;
}
