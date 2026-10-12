/* gate: port__tools_mcp_tool_py___wait_for_reconnect_or_shutdown — traced copy of mct_wait_for_reconnect_or_shutdown (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int mct_wait_for_reconnect_or_shutdown(const char *server_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_wait_for_reconnect_or_shutdown <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_wait_for_reconnect_or_shutdown");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = mct_wait_for_reconnect_or_shutdown(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_wait_for_reconnect_or_shutdown");
    }
    return 0;
}
