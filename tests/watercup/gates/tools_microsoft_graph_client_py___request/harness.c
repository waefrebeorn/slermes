/* gate: tools_microsoft_graph_client_py___request — traced copy of tools_microsoft_graph_client_u_request */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_microsoft_graph_client_u_request(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_microsoft_graph_client_u_request");
    int result = tools_microsoft_graph_client_u_request(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_microsoft_graph_client_u_request");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_microsoft_graph_client_u_request <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
