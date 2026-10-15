/* gate: port__tools_skill_ledger_py__record_mutation — traced copy of record_mutation (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * record_mutation(const char *action, const char *skill,
                      const char *before_root, const char *before_json,
                      const char *after_root, const char *actor,
                      const char *evidence_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_mutation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("record_mutation");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = record_mutation((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("record_mutation");
    }
    return 0;
}
