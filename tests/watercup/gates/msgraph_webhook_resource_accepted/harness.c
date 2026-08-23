/* gate: msgraph_webhook_resource_accepted — traced copy of
 * src/gateway/platforms/msgraph_webhook/msgraph_webhook_security.c::
 *   msgraph_resource_accepted (+ msgraph_normalize_resource_value, pattern_matches)
 * (PoP: gateway/platforms/msgraph_webhook.py:_resource_accepted).
 *
 * Input encoding: "<resource>|<pattern,pattern,...>" (empty resource ok).
 * Rungs: no-patterns? -> per-pattern loop -> empty-normalized-pattern? ->
 *        wildcard? -> eq/prefix match. Value: "true"/"false".
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static char *normalize_resource_value(const char *resource) {
    /* strip(), then strip leading/trailing '/' */
    while (*resource == ' ' || *resource == '\t' || *resource == '\n' || *resource == '\r') resource++;
    size_t len = strlen(resource);
    while (len > 0 && (resource[len-1]==' '||resource[len-1]=='\t'||resource[len-1]=='\n'||resource[len-1]=='\r')) len--;
    char *s = strndup(resource, len);
    size_t a = 0;
    while (a < len && s[a] == '/') a++;
    size_t e = len;
    while (e > a && s[e-1] == '/') e--;
    char *out = strndup(s + a, e - a);
    free(s);
    return out;
}

static int pattern_matches(const char *nr, const char *pattern) {
    size_t plen = strlen(pattern);
    if (plen == 0) return 0;
    if (pattern[plen-1] == '*') {
        char prefix[1024];
        size_t pfx = plen - 1;
        while (pfx > 0 && pattern[pfx-1] == '/') pfx--;
        if (pfx >= sizeof(prefix)) pfx = sizeof(prefix) - 1;
        memcpy(prefix, pattern, pfx);
        prefix[pfx] = '\0';
        if (strcmp(nr, prefix) == 0) return 1;
        size_t nlen = strlen(nr);
        if (nlen >= pfx + 1 && strncmp(nr, prefix, pfx) == 0 && nr[pfx] == '/') return 1;
        return 0;
    }
    if (strcmp(nr, pattern) == 0) return 1;
    size_t nlen = strlen(nr), pl = strlen(pattern);
    if (nlen >= pl + 1 && strncmp(nr, pattern, pl) == 0 && nr[pl] == '/') return 1;
    return 0;
}

static void resource_accepted(const char *resource, const char *patterns_str) {
    WCUP_BEGIN("resource_accepted");
    int ok;
    if (!patterns_str || !*patterns_str) {
        WCUP_BRANCH(1);
        ok = 1;
    } else {
        WCUP_BRANCH(0);
        char *nr = normalize_resource_value(resource ? resource : "");
        char patterns[64][256];
        int np = 0;
        const char *p = patterns_str;
        while (*p && np < 64) {
            while (*p && (*p == '\n' || *p == ',' || isspace((unsigned char)*p))) p++;
            if (!*p) break;
            int k = 0;
            while (*p && k < 255 && *p != '\n' && *p != ',' && !isspace((unsigned char)*p))
                patterns[np][k++] = *p++;
            patterns[np][k] = '\0';
            if (k > 0) np++;
        }
        ok = 0;
        for (int i = 0; i < np; i++) {
            WCUP_LOOP_TICK();
            char *np_norm = normalize_resource_value(patterns[i]);
            if (np_norm && *np_norm) {
                WCUP_BRANCH(1);
                if (pattern_matches(nr, np_norm)) ok = 1;
            } else {
                WCUP_BRANCH(0);
            }
            free(np_norm);
            if (ok) break;
        }
        free(nr);
    }
    WCUP_VALSTR(ok ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resource_accepted");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resource_accepted '<res>|<pats>'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *bar = strchr(arg, '|');
        const char *pats = "";
        if (bar) { *bar = '\0'; pats = bar + 1; }
        resource_accepted(arg, pats);
        free(arg);
    }
    return 0;
}
