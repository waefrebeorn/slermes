/* gate: port__tools_skill_ledger_py__rollback_entry — traced copy of rollback_entry (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool rollback_entry(const char *entry_id, char *out_msg, size_t msg_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rollback_entry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rollback_entry");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = rollback_entry(argv[i], argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rollback_entry");
    }
    return 0;
}
