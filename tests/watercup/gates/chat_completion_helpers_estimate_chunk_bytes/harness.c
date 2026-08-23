/* gate: chat_completion_helpers_estimate_chunk_bytes — traced copy of
 * src/agent/chat_completion_helpers_ports.c::cch_estimate_chunk_bytes.
 * LIVE twin: agent/chat_completion_helpers.py::_estimate_chunk_bytes
 * (called on a real chunk object built by twin.py).
 *
 * The C wrapper prints its result internally, so a traced copy carries the
 * value into the cup instead of stdout (one cup line per input, argv-safe).
 *
 * Shared ladder (must match twin.py EXACTLY, same rung order):
 *   V1 content_len / V2 reasoning_len / V3 tool_args_len (decimal)
 *   B1 any payload present (size != framing floor)
 *   V4 final size (decimal)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static long trace_estimate(const char *arg) {
    WCUP_BEGIN("estimate_chunk_bytes");
    long size = 40;
    const char *t1 = arg ? strchr(arg, '\t') : NULL;
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long cl = atol(arg ? arg : "");
    long rl = t1 ? atol(t1 + 1) : 0;
    long tl = t2 ? atol(t2 + 1) : 0;
    size += cl;
    if (t1) size += rl;
    if (t2) size += tl;

    char b[32];
    snprintf(b, sizeof b, "%ld", cl); WCUP_VALSTR(b);   /* V1 */
    snprintf(b, sizeof b, "%ld", rl); WCUP_VALSTR(b);   /* V2 */
    snprintf(b, sizeof b, "%ld", tl); WCUP_VALSTR(b);   /* V3 */
    WCUP_BRANCH(size != 40);                            /* B1 */
    snprintf(b, sizeof b, "%ld", size); WCUP_VALSTR(b); /* V4 */
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("estimate_chunk_bytes");
    return size;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s estimate_chunk_bytes <content\\treasoning\\ttoolargs>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_estimate(argv[i]);
    return 0;
}
