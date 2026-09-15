/* gate: redact_redact_query_string — links the REAL C port
 * src/agent/redact_ports.c::red_redact_query_string (post-parity-fix).
 * LIVE twin: agent/redact.py::_redact_query_string.
 *
 * Input argv: one query string per input.
 * Branch ladder (must match twin.py EXACTLY): per pair, in order —
 *   B: '=' present   B: key sensitive   (emitted for every pair)
 *   VSTR(redacted query)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"

char *red_redact_query_string(const char *query);

static bool key_sensitive(const char *pair, size_t len) {
    char key[256];
    if (len >= sizeof key) return false;
    memcpy(key, pair, len); key[len] = '\0';
    for (char *p = key; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *sensitive[] = {"access_token", "refresh_token", "id_token",
        "token", "api_key", "apikey", "client_secret", "password", "auth",
        "jwt", "session", "secret", "key", "code", "signature",
        "x-amz-signature", NULL};
    for (int i = 0; sensitive[i]; i++)
        if (strcmp(key, sensitive[i]) == 0) return true;
    return false;
}

static void run_one(const char *query) {
    WCUP_BEGIN("_redact_query_string");
    /* trace the per-pair ladder */
    const char *p = query;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t seg = amp ? (size_t)(amp - p) : strlen(p);
        const char *eq = memchr(p, '=', seg);
        bool has_eq = eq != NULL;
        bool sens = has_eq && key_sensitive(p, (size_t)(eq - p));
        WCUP_BRANCH(has_eq);
        WCUP_BRANCH(sens);
        if (!amp) break;
        p = amp + 1;
    }

    char *r = red_redact_query_string(query ? query : "");
    WCUP_VALSTR(r ? r : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    WCUP_END("_redact_query_string");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <query...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
