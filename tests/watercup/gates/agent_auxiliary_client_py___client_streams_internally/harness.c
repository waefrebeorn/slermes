/* gate: agent_auxiliary_client_py___client_streams_internally — traced copy of aux__client_streams_internally */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__client_streams_internally(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__client_streams_internally");
    int result = aux__client_streams_internally(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__client_streams_internally");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__client_streams_internally <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
