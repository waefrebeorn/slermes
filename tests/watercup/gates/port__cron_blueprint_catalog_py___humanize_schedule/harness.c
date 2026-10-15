/* gate: port__cron_blueprint_catalog_py___humanize_schedule — traced copy of blueprint_catalog_humanize_schedule (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * blueprint_catalog_humanize_schedule(const char *schedule_template, const char *slots_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s blueprint_catalog_humanize_schedule <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("blueprint_catalog_humanize_schedule");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = blueprint_catalog_humanize_schedule(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("blueprint_catalog_humanize_schedule");
    }
    return 0;
}
