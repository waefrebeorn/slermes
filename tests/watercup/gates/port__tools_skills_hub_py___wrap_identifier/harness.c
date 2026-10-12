/* gate: port__tools_skills_hub_py___wrap_identifier — traced copy of skills_hub_wrap_identifier (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool skills_hub_wrap_identifier(const char *query, const char *text);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_hub_wrap_identifier <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("skills_hub_wrap_identifier");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = skills_hub_wrap_identifier(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("skills_hub_wrap_identifier");
    }
    return 0;
}
