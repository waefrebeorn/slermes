/* gate: port__hermes_cli_models_py___should_use_copilot_responses_api — traced copy of should_use_copilot_responses_api (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int should_use_copilot_responses_api(const char *model_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_use_copilot_responses_api <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("should_use_copilot_responses_api");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = should_use_copilot_responses_api(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("should_use_copilot_responses_api");
    }
    return 0;
}
