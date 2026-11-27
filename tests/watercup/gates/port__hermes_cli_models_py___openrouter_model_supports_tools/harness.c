/* gate: port__hermes_cli_models_py___openrouter_model_supports_tools — traced copy of openrouter_model_supports_tools (json_param) */
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


extern bool openrouter_model_supports_tools(const json_node_t *item);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s openrouter_model_supports_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("openrouter_model_supports_tools");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = openrouter_model_supports_tools(_j0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("openrouter_model_supports_tools");
    }
    return 0;
}
