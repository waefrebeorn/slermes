/* gate: agent_agent_init_py___normalize_custom_provider_name -> real string port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_agent_init_u_normalize_custom_provider_name(const char *value);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_agent_init_u_normalize_custom_provider_name");
        fflush(stdout);
        char *r = agent_agent_init_u_normalize_custom_provider_name(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_agent_init_u_normalize_custom_provider_name");
        free(r);
    }
    return 0;
}
