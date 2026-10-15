/* gate: port__hermes_cli_plugin_capabilities_py__declared_set_changed — traced copy of pc_declared_set_changed (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int pc_declared_set_changed(const char *plugin_id, const char *declared_json,
                            const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pc_declared_set_changed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pc_declared_set_changed");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = pc_declared_set_changed((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pc_declared_set_changed");
    }
    return 0;
}
