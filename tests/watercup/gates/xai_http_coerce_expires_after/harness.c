/* gate: xai_http_coerce_expires_after — traced copy of
 * src/tools/xai_http.c::cli_tools_xai_http__coerce_expires_after
 * (PoP: tools/xai_http.py:_coerce_expires_after).
 *
 * Rungs: null? -> keyword-set ("",default,none,null,never,permanent,forever,0)?
 *        -> int-parse? -> <=0? -> clamp to MAX. Values: decimal | "null" | SAFE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

#define MAX_EXP  (30L * 24 * 60 * 60)
#define SAFE_EXP (2L * 24 * 60 * 60)

static void emit(wcup_t *wc, const char *s) {
    char cup[32];
    wcup_valstr(wc, s);
    wcup_finalize(wc, cup, sizeof cup);
    printf("%s\n", cup);
}

static void coerce_expires_after(const char *value) {
    WCUP_BEGIN("coerce_expires_after");
    if (!value) { WCUP_BRANCH(1); emit(&_wc, "null"); return; }
    WCUP_BRANCH(0);
    char buf[256];
    size_t j = 0;
    for (const char *s = value; *s && j + 1 < sizeof(buf); s++)
        buf[j++] = (char)tolower((unsigned char)*s);
    buf[j] = '\0';
    char *b = buf;
    while (*b == ' ' || *b == '\t') b++;
    size_t L = strlen(b);
    while (L > 0 && (b[L-1] == ' ' || b[L-1] == '\t')) b[--L] = '\0';

    if (b[0] == '\0' || strcmp(b, "default") == 0 || strcmp(b, "none") == 0
        || strcmp(b, "null") == 0 || strcmp(b, "never") == 0
        || strcmp(b, "permanent") == 0 || strcmp(b, "forever") == 0
        || strcmp(b, "0") == 0) {
        WCUP_BRANCH(1);
        emit(&_wc, "null");
        return;
    }
    WCUP_BRANCH(0);

    char *endp = NULL;
    long seconds = strtol(b, &endp, 10);
    if (endp != b && (*endp == '\0' || *endp == ' ' || *endp == '\t')) {
        WCUP_BRANCH(1);
        if (seconds <= 0) { WCUP_BRANCH(1); emit(&_wc, "null"); return; }
        WCUP_BRANCH(0);
        if (seconds > MAX_EXP) seconds = MAX_EXP;
        char out[32];
        snprintf(out, sizeof(out), "%ld", seconds);
        emit(&_wc, out);
        return;
    }
    WCUP_BRANCH(0);
    /* Unparseable -> SAFE default */
    char out2[32];
    snprintf(out2, sizeof(out2), "%ld", SAFE_EXP);
    emit(&_wc, out2);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_expires_after <val>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) coerce_expires_after(argv[i]);
    return 0;
}
