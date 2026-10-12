/* gate: port__tools_skills_hub_py___find_skill_dir — traced copy of su_find_skill_dir (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool su_find_skill_dir(const char *hermes_home, const char *skill_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_find_skill_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("su_find_skill_dir");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = su_find_skill_dir(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("su_find_skill_dir");
    }
    return 0;
}
