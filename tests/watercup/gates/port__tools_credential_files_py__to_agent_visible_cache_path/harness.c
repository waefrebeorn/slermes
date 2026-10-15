/* gate: port__tools_credential_files_py__to_agent_visible_cache_path — traced copy of cli_tools_credential_files_to_agent_visible_cache_path (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char* cli_tools_credential_files_to_agent_visible_cache_path(const char *host_path, const char *container_base,
                                                               char *buf, size_t bufsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_credential_files_to_agent_visible_cache_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_credential_files_to_agent_visible_cache_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_tools_credential_files_to_agent_visible_cache_path((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_credential_files_to_agent_visible_cache_path");
    }
    return 0;
}
