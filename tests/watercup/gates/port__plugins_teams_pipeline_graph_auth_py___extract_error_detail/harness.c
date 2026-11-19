/* gate: port__tools_microsoft_graph_auth_py___extract_error_detail — traced copy of cli_tools_microsoft_graph_auth__extract_error_detail (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_microsoft_graph_auth__extract_error_detail(const char *response_body, char *buf, size_t bufsize);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_microsoft_graph_auth__extract_error_detail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_microsoft_graph_auth__extract_error_detail");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_microsoft_graph_auth__extract_error_detail(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_microsoft_graph_auth__extract_error_detail");
    }
    return 0;
}
