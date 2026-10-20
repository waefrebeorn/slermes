/* gate: port__agent_anthropic_adapter_py__build_anthropic_kwargs — traced copy of antr_build_anthropic_kwargs (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * antr_build_anthropic_kwargs(const char *model, long max_tokens, const char *system_json,
                                  const char *messages_json, const char *tools_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_build_anthropic_kwargs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("antr_build_anthropic_kwargs");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = antr_build_anthropic_kwargs(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("antr_build_anthropic_kwargs");
    }
    return 0;
}
