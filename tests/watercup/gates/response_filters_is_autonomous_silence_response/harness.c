/* gate: response_filters_is_autonomous_silence_response — traced C copy of
 * src/gateway/response_filters.c::cli_gateway_response_filters_is_autonomous_silence_response
 * (PoP of LIVE gateway/response_filters.py::is_autonomous_silence_response).
 * V-events are emitted only for line tokens (no embedded newlines) so C and
 * LIVE canonicalization agree byte-for-byte.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static const char *MARKERS[] = { "[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", NULL };

/* Port of _canonical_silence_candidate: strip, uppercase, collapse spaces. */
static char *canon(const char *text) {
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    char *out = (char *)malloc(len + 1);
    int j = 0, in_space = 0;
    for (const char *p = text; *p; p++) {
        if (*p == ' ' || *p == '\t') {
            if (!in_space && j > 0) out[j++] = ' ', in_space = 1;
        } else {
            out[j++] = (char)toupper((unsigned char)*p);
            in_space = 0;
        }
    }
    if (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    return out;
}

static int is_marker(const char *c) {
    for (int i = 0; MARKERS[i]; i++)
        if (strcmp(c, MARKERS[i]) == 0) return 1;
    return 0;
}

static int is_autonomous_silence_response(const char *response) {
    WCUP_BEGIN("is_autonomous_silence_response");
    int result = 0;
    if (!response) { WCUP_BRANCH(1); goto done; }
    WCUP_BRANCH(0);
    {
        const char *s = response;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t sl = strlen(s);
        while (sl > 0 && isspace((unsigned char)s[sl - 1])) sl--;
        if (sl == 0) { WCUP_BRANCH(1); goto done; }
        WCUP_BRANCH(0);

        /* Whole-response token check (no V event: stripped text may embed \n). */
        {
            char *tmp = strndup(s, sl);
            char *c0 = canon(tmp);
            int whole = is_marker(c0);
            free(c0); free(tmp);
            if (whole) { WCUP_BRANCH(1); result = 1; goto done; }
            WCUP_BRANCH(0);
        }

        /* First + last non-empty line tokens. */
        int first_hit = 0, last_hit = 0;
        char *copy = strndup(s, sl);
        char *first = NULL, *last = NULL;
        for (char *ln = strtok(copy, "\n"); ln; ln = strtok(NULL, "\n")) {
            if (!*ln) continue;
            if (!first) first = ln;
            last = ln;
        }
        if (first) {
            WCUP_BRANCH(1);
            char *c1 = canon(first);
            WCUP_VALSTR(c1);
            first_hit = is_marker(c1);
            free(c1);
        } else WCUP_BRANCH(0);
        if (last && last != first) {
            WCUP_BRANCH(1);
            char *c2 = canon(last);
            WCUP_VALSTR(c2);
            last_hit = is_marker(c2);
            free(c2);
        } else WCUP_BRANCH(0);
        free(copy);
        if (first_hit || last_hit) { WCUP_BRANCH(1); result = 1; goto done; }
        WCUP_BRANCH(0);

        /* Bracketed sentinel same-line prefix. */
        if (sl >= 8 && strncasecmp(s, "[SILENT]", 8) == 0) {
            WCUP_BRANCH(1); result = 1; goto done;
        }
        WCUP_BRANCH(0);
    }
done:
    {
        char buf[4];
        snprintf(buf, sizeof buf, "%d", result);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_autonomous_silence_response");
    }
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_autonomous_silence_response <str>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        is_autonomous_silence_response(strcmp(argv[i], "null") == 0 ? NULL : argv[i]);
    return 0;
}
