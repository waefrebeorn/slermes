/* gate: port__tools_skills_hub_py___coerce_skill_payload — traced copy of sh_coerce_skill_payload (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * sh_coerce_skill_payload(const char *data_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sh_coerce_skill_payload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sh_coerce_skill_payload");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = sh_coerce_skill_payload(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sh_coerce_skill_payload");
    }
    return 0;
}
