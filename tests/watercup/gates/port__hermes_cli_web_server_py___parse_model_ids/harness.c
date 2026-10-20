/* gate: port__hermes_cli_web_server_py___parse_model_ids — traced copy of web_parse_model_ids (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** web_parse_model_ids(const char *json_response, size_t *out_count);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_parse_model_ids <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("web_parse_model_ids");
    size_t _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = web_parse_model_ids(argv[i], &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("web_parse_model_ids");
    }
    return 0;
}
