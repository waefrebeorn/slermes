/* gate: port__hermes_cli_fallback_cmd_py___describe_primary — traced copy of fallback_describe_primary (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
typedef json_t json_node_t;
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern void fallback_describe_primary(const json_t *config, char *out, size_t out_cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fallback_describe_primary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fallback_describe_primary");
    char _ob[4096]; _ob[0] = '\0';
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    fallback_describe_primary(_j0, _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);

    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fallback_describe_primary");
    }
    return 0;
}
