/* gate: port__hermes_cli_update_cmd_py___is_zip_preserved_entry_status_line — traced copy of _is_zip_preserved_entry_status_line (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _is_zip_preserved_entry_status_line(const char *line);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_zip_preserved_entry_status_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_zip_preserved_entry_status_line");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _is_zip_preserved_entry_status_line(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_zip_preserved_entry_status_line");
    }
    return 0;
}
