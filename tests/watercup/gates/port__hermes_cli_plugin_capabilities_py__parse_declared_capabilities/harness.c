/* gate: port__hermes_cli_plugin_capabilities_py__parse_declared_capabilities — traced copy of pc_parse_declared_capabilities (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * pc_parse_declared_capabilities(const char *raw_json, const char *plugin_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pc_parse_declared_capabilities <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pc_parse_declared_capabilities");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = pc_parse_declared_capabilities(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pc_parse_declared_capabilities");
    }
    return 0;
}
