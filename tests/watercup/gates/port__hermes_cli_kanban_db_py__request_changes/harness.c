/* gate: port__hermes_cli_kanban_db_py__request_changes — traced copy of kanban_request_changes (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int kanban_request_changes(const char *task_id, const char *reason,
                            char *out_impl, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kanban_request_changes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kanban_request_changes");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = kanban_request_changes(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kanban_request_changes");
    }
    return 0;
}
