/* gate: port__tools_discord_tool_py___enrich_403 — traced copy of discord_enrich_403 (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * discord_enrich_403(const char *error_body);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s discord_enrich_403 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("discord_enrich_403");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = discord_enrich_403(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("discord_enrich_403");
    }
    return 0;
}
