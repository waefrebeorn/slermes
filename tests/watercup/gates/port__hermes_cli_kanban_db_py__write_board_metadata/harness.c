/* gate: port__hermes_cli_kanban_db_py__write_board_metadata — traced copy of kdb_write_board_metadata (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * kdb_write_board_metadata(const char *board,
                               const char *name, const char *description,
                               const char *icon, const char *color,
                               int archived_set, int archived,
                               const char *default_workdir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_write_board_metadata <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kdb_write_board_metadata");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = kdb_write_board_metadata((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 7) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kdb_write_board_metadata");
    }
    return 0;
}
