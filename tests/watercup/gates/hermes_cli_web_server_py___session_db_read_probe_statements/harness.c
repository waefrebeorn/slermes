/* gate: hermes_cli_web_server_py___session_db_read_probe_statements — traced copy of ws_session_db_read_probe_statements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_session_db_read_probe_statements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_session_db_read_probe_statements");
    int result = ws_session_db_read_probe_statements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_session_db_read_probe_statements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_session_db_read_probe_statements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
