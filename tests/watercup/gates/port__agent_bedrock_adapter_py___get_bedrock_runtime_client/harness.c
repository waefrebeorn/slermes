/* gate: port__agent_bedrock_adapter_py___get_bedrock_runtime_client — traced copy of cli_agent_bedrock_adapter__get_bedrock_runtime_client (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_agent_bedrock_adapter__get_bedrock_runtime_client(const char *region, char *buf, size_t bufsize);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_bedrock_adapter__get_bedrock_runtime_client <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_bedrock_adapter__get_bedrock_runtime_client");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_agent_bedrock_adapter__get_bedrock_runtime_client(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_bedrock_adapter__get_bedrock_runtime_client");
    }
    return 0;
}
