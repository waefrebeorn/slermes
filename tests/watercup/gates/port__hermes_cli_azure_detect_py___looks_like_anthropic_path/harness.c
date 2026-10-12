/* gate: port__hermes_cli_azure_detect_py___looks_like_anthropic_path — traced copy of azure_looks_like_anthropic_path (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int azure_looks_like_anthropic_path(const char *url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s azure_looks_like_anthropic_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("azure_looks_like_anthropic_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = azure_looks_like_anthropic_path(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("azure_looks_like_anthropic_path");
    }
    return 0;
}
