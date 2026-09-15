/* gate: env_passthrough_register_ladder — drives the REAL
 * lib/libenvpassthrough/env_passthrough.c through the registration ladder.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: name empty after strip        -> skip (not registered)
 *   B2: name is a Hermes provider credential (static blocklist or dynamic
 *       internal-secret pattern)      -> refused
 *   B3: name already registered       -> idempotent true
 *   B4: capacity available            -> registered true
 *   V1: post-probe allowlist joined with ","
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "env_passthrough.h"

/* Dynamic Hermes-internal secrets (LIVE _is_hermes_internal_secret):
 *   AUXILIARY_*_API_KEY | AUXILIARY_*_BASE_URL
 *   GATEWAY_RELAY_*_SECRET | GATEWAY_RELAY_*_KEY | GATEWAY_RELAY_*_TOKEN */
static bool wc_is_hermes_internal_secret(const char *name) {
    char up[256];
    size_t i = 0;
    for (; name[i] && i < sizeof(up) - 1; i++)
        up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = '\0';
    size_t len = strlen(up);
    if (strncmp(up, "AUXILIARY_", 10) == 0 &&
        (len >= 8 && strcmp(up + len - 8, "_API_KEY") == 0))
        return true;
    if (strncmp(up, "AUXILIARY_", 10) == 0 &&
        (len >= 9 && strcmp(up + len - 9, "_BASE_URL") == 0))
        return true;
    if (strncmp(up, "GATEWAY_RELAY_", 14) == 0) {
        const char *suf[3] = {"_SECRET", "_KEY", "_TOKEN"};
        for (int s = 0; s < 3; s++) {
            size_t sl = strlen(suf[s]);
            if (len >= sl && strcmp(up + len - sl, suf[s]) == 0)
                return true;
        }
    }
    return false;
}

static void trace_probe(const char *raw) {
    WCUP_BEGIN("env_passthrough_register");
    bool registered = false;
    /* strip whitespace */
    char name[256];
    const char *s = raw ? raw : "";
    while (*s == ' ' || *s == '\t') s++;
    snprintf(name, sizeof(name), "%s", s);
    size_t nl = strlen(name);
    while (nl > 0 && (name[nl-1]==' ' || name[nl-1]=='\t')) name[--nl] = '\0';

    if (!name[0]) {                                    /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    if (env_passthrough_is_blocked(name) ||
        wc_is_hermes_internal_secret(name)) {          /* B2 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    if (env_passthrough_is_allowed(name)) {            /* B3 */
        WCUP_BRANCH(1);
        registered = true;
        goto out;
    }
    WCUP_BRANCH(0);

    registered = env_passthrough_register(name);       /* B4 */
    WCUP_BRANCH(registered);

out:;
    /* V1: current allowlist snapshot */
    char snap[4096]; size_t pos = 0;
    char **list = NULL; int cnt = 0;
    env_passthrough_get_all(&list, &cnt);
    /* match the twin's canonical (sorted) snapshot encoding */
    for (int i = 2; i < cnt; i++) {
        for (int j = i; j > 0 && strcmp(list[j-1], list[j]) > 0; j--) {
            char *t = list[j-1]; list[j-1] = list[j]; list[j] = t;
        }
    }
    for (int i = 0; i < cnt && pos < sizeof(snap) - 2; i++) {
        if (pos) snap[pos++] = ',';
        size_t l = strlen(list[i]);
        if (l > sizeof(snap) - pos - 1) l = sizeof(snap) - pos - 1;
        memcpy(snap + pos, list[i], l);
        pos += l;
    }
    snap[pos] = '\0';
    env_passthrough_free_list(list, cnt);
    WCUP_VALSTR(snap);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("env_passthrough_register");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_env_register <name>...\n", argv[0]); return 2; }
    env_passthrough_clear();
    for (int i = 2; i < argc; i++)
        trace_probe(argv[i]);
    return 0;
}
