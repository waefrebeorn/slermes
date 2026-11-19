/* gate: port__hermes_cli_models_py___resolve_static_model_alias — traced copy of resolve_static_model_alias (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int resolve_static_model_alias(const char *name_lower,
                                      char *provider_out, size_t poutsz,
                                      char *model_out, size_t moutsz,
                                      int current_only);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_static_model_alias <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("resolve_static_model_alias");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = resolve_static_model_alias(argv[i], argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], (long)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_static_model_alias");
    }
    return 0;
}
