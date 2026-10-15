/* gate: port__tools_skills_hub_py___resolve_skill_md_url — traced copy of skills_hub_resolve_skill_md_url (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * skills_hub_resolve_skill_md_url(const char *identifier);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_hub_resolve_skill_md_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("skills_hub_resolve_skill_md_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = skills_hub_resolve_skill_md_url(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("skills_hub_resolve_skill_md_url");
    }
    return 0;
}
