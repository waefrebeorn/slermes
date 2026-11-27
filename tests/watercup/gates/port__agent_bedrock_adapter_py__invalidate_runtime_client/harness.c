/* gate: port__agent_bedrock_adapter_py__invalidate_runtime_client — traced copy of invalidate_runtime_client (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool invalidate_runtime_client(const char *region);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s invalidate_runtime_client <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("invalidate_runtime_client");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = invalidate_runtime_client(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("invalidate_runtime_client");
    }
    return 0;
}
