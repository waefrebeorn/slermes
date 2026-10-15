/* gate: port__hermes_cli_auth_py___oauth_trace — traced copy of auth_oauth_trace (json_param) */
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


extern void auth_oauth_trace(const char *event, const char *sequence_id, const json_t *fields);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_oauth_trace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_oauth_trace");
    json_t *_j2 = json_parse(argv[2 + (i - 2 + 2) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = auth_oauth_trace(argv[i], argv[2 + (i - 2 + 1) % 5], _j2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j2) json_free(_j2);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_oauth_trace");
    }
    return 0;
}
