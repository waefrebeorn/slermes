/* gate: port__hermes_cli_banner_py__save_banner_snapshot — traced copy of banner_save_snapshot (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void banner_save_snapshot(const char *tools_json, const char *enabled_toolsets_json,
                          const char *availability_json, const char *toolset_map_json,
                          const char *skills_by_category_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s banner_save_snapshot <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("banner_save_snapshot");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    banner_save_snapshot();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("banner_save_snapshot");
    }
    return 0;
}
