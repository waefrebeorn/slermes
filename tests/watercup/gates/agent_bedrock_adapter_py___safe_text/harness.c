/* gate: agent_bedrock_adapter_py___safe_text -> real string-returning port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_bedrock_adapter_u_safe_text(const char *text);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_bedrock_adapter_u_safe_text");
        fflush(stdout);
        char *r = agent_bedrock_adapter_u_safe_text(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_bedrock_adapter_u_safe_text");
        free(r);
    }
    return 0;
}
