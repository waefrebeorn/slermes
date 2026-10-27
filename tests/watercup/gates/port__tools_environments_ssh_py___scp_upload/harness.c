/* gate: port__tools_environments_ssh_py___scp_upload — traced copy of cli_tools_environments_ssh__scp_upload (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_environments_ssh__scp_upload(const char *host_path, const char *remote_path,
                                            const char *user, const char *host,
                                            int port, const char *key_path,
                                            const char *control_socket);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_environments_ssh__scp_upload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_environments_ssh__scp_upload");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_environments_ssh__scp_upload(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), argv[2 + (i - 2 + 5) % 5], argv[2 + (i - 2 + 6) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_environments_ssh__scp_upload");
    }
    return 0;
}
