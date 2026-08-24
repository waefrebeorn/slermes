/*
 * stream_single_writer.c — canonical C11 port of agent/stream_single_writer.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_stream_single_writer_claim_stream_writer @ agent/stream_single_writer.py:claim_stream_writer */
int agent_stream_single_writer_claim_stream_writer(const char *arg) {
    /* Python: monotonic token or 0 (unfenced). Arg = "token\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') { printf("%s\n", arg); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: agent_stream_single_writer_stream_writer_is_current @ agent/stream_single_writer.py:stream_writer_is_current */
int agent_stream_single_writer_stream_writer_is_current(const char *arg) {
    /* Python: falsy token -> True; else delegate check. Arg =
     * "token\tcurrent\tstate". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strcmp(arg, "0") == 0) { printf("1\n"); return 0; }
    int state = t2 && t2[1] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", (t1 && t1[1] == '1') ? "1" : "0");
    return 0;
}
