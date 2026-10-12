/* gate: port__hermes_cli_update_cmd_py___is_zip_staging_artifact_status_line — traced copy of upd_is_zip_staging_artifact_status_line (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool upd_is_zip_staging_artifact_status_line(const char *line);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_is_zip_staging_artifact_status_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("upd_is_zip_staging_artifact_status_line");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = upd_is_zip_staging_artifact_status_line(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("upd_is_zip_staging_artifact_status_line");
    }
    return 0;
}
