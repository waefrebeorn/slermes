/* gate: file_safety_find_sandbox_mirror_segments — links the REAL C port
 * src/agent/file_safety_ports.c::fsf_find_sandbox_mirror_segments (post
 * parity-fix: walks path segments like LIVE's parts tuple instead of
 * substring-counting ".hermes").
 * LIVE twin: agent/file_safety.py::_find_sandbox_mirror_segments.
 *
 * Input argv: one absolute path per input.
 * Branch ladder (must match twin.py EXACTLY): per segment i —
 *   B: parts[i]=="sandboxes"   B: shape check passes (i+5 < n &&
 *   home/.hermes at +3/+4)     (emitted only when B1 true, LIVE `continue`)
 *   VSTR(index or "-1")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

char *fsf_find_sandbox_mirror_segments(const char *path);

static void run_one(const char *path) {
    /* split once for tracing */
    size_t n = 0;
    static char *parts[256];
    static char buf[4096];
    snprintf(buf, sizeof buf, "%s", path ? path : "");
    char *tok = buf;
    while (*tok == '/') tok++;
    while (*tok && n < 256) {
        char *slash = strchr(tok, '/');
        size_t len = slash ? (size_t)(slash - tok) : strlen(tok);
        if (len == 0) { tok++; continue; }
        parts[n] = strndup(tok, len);
        n++;
        if (!slash) break;
        tok = slash + 1;
    }

    WCUP_BEGIN("_find_sandbox_mirror_segments");
    for (size_t i = 0; i < n; i++) {
        bool sb = strcmp(parts[i], "sandboxes") == 0;
        WCUP_BRANCH(sb);
        if (!sb) continue;
        bool shape = ((long)i + 5 < (long)n) &&
                     strcmp(parts[i + 3], "home") == 0 &&
                     strcmp(parts[i + 4], ".hermes") == 0;
        WCUP_BRANCH(shape);
    }

    char *r = fsf_find_sandbox_mirror_segments(path);
    WCUP_VALSTR(r ? r : "-1");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    for (size_t i = 0; i < n; i++) free(parts[i]);
    WCUP_END("_find_sandbox_mirror_segments");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <path...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
