/* gate: port__agent_anthropic_adapter_py__build_anthropic_client — traced copy of antr_build_anthropic_client (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * antr_build_anthropic_client(const char *api_key, const char *base_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_build_anthropic_client <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("antr_build_anthropic_client");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = antr_build_anthropic_client(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("antr_build_anthropic_client");
    }
    return 0;
}
