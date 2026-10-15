/* gate: port__hermes_cli_models_py___cache_entry_valid — traced copy of models_cache_entry_valid (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int models_cache_entry_valid(const char *entry_json, const char *fp);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_cache_entry_valid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_cache_entry_valid");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = models_cache_entry_valid(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_cache_entry_valid");
    }
    return 0;
}
