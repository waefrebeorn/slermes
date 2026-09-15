/* gate: agent_billing_view_py___parse_auto_reload_card — traced copy of agent_billing_view_u_parse_auto_reload_card */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_billing_view_u_parse_auto_reload_card(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_billing_view_u_parse_auto_reload_card");
    int result = agent_billing_view_u_parse_auto_reload_card(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_billing_view_u_parse_auto_reload_card");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_billing_view_u_parse_auto_reload_card <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
