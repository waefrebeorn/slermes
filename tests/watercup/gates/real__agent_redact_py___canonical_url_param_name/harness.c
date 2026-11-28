/* gate: real__agent_redact_py___canonical_url_param_name -> real canonical_url_param_name (owned string). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_redact_u_canonical_url_param_name(const char *name);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <name>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_redact_u_canonical_url_param_name");
        fflush(stdout);
        char *r = agent_redact_u_canonical_url_param_name(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("agent_redact_u_canonical_url_param_name");
        free(r);
    }
    return 0;
}
