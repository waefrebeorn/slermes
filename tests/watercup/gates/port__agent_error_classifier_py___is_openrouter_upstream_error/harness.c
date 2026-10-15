/* gate: port__agent_error_classifier_py___is_openrouter_upstream_error — traced copy of cli_agent_error_classifier__is_openrouter_upstream_error (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_agent_error_classifier__is_openrouter_upstream_error(const char *body_json, const char *provider);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_error_classifier__is_openrouter_upstream_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_error_classifier__is_openrouter_upstream_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_agent_error_classifier__is_openrouter_upstream_error(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_error_classifier__is_openrouter_upstream_error");
    }
    return 0;
}
