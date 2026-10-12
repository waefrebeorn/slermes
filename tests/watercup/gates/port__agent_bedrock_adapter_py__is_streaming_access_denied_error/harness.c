/* gate: port__agent_bedrock_adapter_py__is_streaming_access_denied_error — traced copy of cli_agent_bedrock_adapter_is_streaming_access_denied_error (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_agent_bedrock_adapter_is_streaming_access_denied_error(const char *error_msg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_bedrock_adapter_is_streaming_access_denied_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_bedrock_adapter_is_streaming_access_denied_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_agent_bedrock_adapter_is_streaming_access_denied_error(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_bedrock_adapter_is_streaming_access_denied_error");
    }
    return 0;
}
