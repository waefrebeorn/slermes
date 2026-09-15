/* gate: agent_stream_diag_py__log_stream_retry — traced copy of sdi_log_stream_retry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sdi_log_stream_retry(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("sdi_log_stream_retry");
    int result = sdi_log_stream_retry(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sdi_log_stream_retry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sdi_log_stream_retry <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
