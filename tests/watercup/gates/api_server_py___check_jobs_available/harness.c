/* gate: api_server_py___check_jobs_available — traced copy of apisg_check_jobs_available */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apisg_check_jobs_available(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apisg_check_jobs_available");
    int result = apisg_check_jobs_available(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apisg_check_jobs_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apisg_check_jobs_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
