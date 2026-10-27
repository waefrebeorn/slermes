/* gate: port__hermes_cli_moa_config_py__build_moa_turn_prompt — traced copy of moa_build_turn_prompt (json_param) */
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


extern char * moa_build_turn_prompt(const char *user_prompt, const json_t *config, const char *preset);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_build_turn_prompt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("moa_build_turn_prompt");
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = moa_build_turn_prompt(argv[i], _j1, argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j1) json_free(_j1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("moa_build_turn_prompt");
    }
    return 0;
}
