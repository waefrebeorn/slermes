/* gate: port__hermes_cli_plugin_packs_py__validate_config_seed — traced copy of plugin_pack_validate_config_seed (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * plugin_pack_validate_config_seed(const char *plugin_id, const char *seed_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s plugin_pack_validate_config_seed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("plugin_pack_validate_config_seed");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = plugin_pack_validate_config_seed(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("plugin_pack_validate_config_seed");
    }
    return 0;
}
