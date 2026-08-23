/* gate: auth_format_auth_error — traced copy of
 * src/hermes_cli/auth.c::format_auth_error (rate-limit code FIXED to
 * "codex_rate_limited" to match LIVE). LIVE twin:
 * hermes_cli/auth.py::format_auth_error.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: no code (non-AuthError input)  -> msg
 *   B2: rate-limited (codex_rate_limited, not relogin) -> msg
 *   B3: relogin_required               -> msg + re-auth hint
 *   B4: subscription_required          -> B5 nous? : fixed string
 *   B6: insufficient_credits           -> B7 nous? : fixed string
 *   B8: temporarily_unavailable        -> msg + retry hint
 *   else                               -> msg
 *
 * Sub-contract: nous-provider entitlement inputs are NOT exercised (LIVE
 * takes a portal/network path there; C returns a static hint — documented
 * divergence, see gate.json). The LIVE-only
 * {subscription_expired,no_usable_credits,...} group is a no-op for
 * non-nous providers on both sides and is traced on neither.
 *
 * Input encoding: "<code|provider|relogin|msg>" ('-' code = NULL;
 * msg must not contain '|').
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

#define CODEX_RATE_LIMITED_CODE "codex_rate_limited"

#define EMIT(out) do { \
    WCUP_VALSTR(out); \
    char cup_[32]; \
    WCUP_FINALIZE(cup_, sizeof cup_); \
    printf("%s\n", cup_); \
    WCUP_END("format_auth_error"); \
} while (0)

static void format_auth_error(const char *code, const char *provider,
                              int relogin, const char *msg) {
    WCUP_BEGIN("format_auth_error");
    const char *m = msg ? msg : "";
    if (!code) {                                   /* B1 */
        WCUP_BRANCH(1);
        EMIT(m);
        return;
    }
    WCUP_BRANCH(0);
    {
        int rl = (relogin == 0) && (strcmp(code, CODEX_RATE_LIMITED_CODE) == 0);
        if (rl) {                                  /* B2 */
            WCUP_BRANCH(1);
            EMIT(m);
            return;
        }
        WCUP_BRANCH(0);
    }
    if (relogin) {                                 /* B3 */
        WCUP_BRANCH(1);
        char rbuf[512];
        snprintf(rbuf, sizeof rbuf, "%s Run `hermes model` to re-authenticate.", m);
        EMIT(rbuf);
        return;
    }
    WCUP_BRANCH(0);
    int nous = provider && strcmp(provider, "nous") == 0;
    if (strcmp(code, "subscription_required") == 0) {   /* B4 */
        WCUP_BRANCH(1);
        if (nous) { WCUP_BRANCH(1); EMIT("Check credits or billing in Nous Portal, then retry."); return; }  /* B5 */
        WCUP_BRANCH(0);
        EMIT("No active paid subscription found. Please purchase/activate a subscription, then retry.");
        return;
    }
    WCUP_BRANCH(0);
    if (strcmp(code, "insufficient_credits") == 0) {    /* B6 */
        WCUP_BRANCH(1);
        if (nous) { WCUP_BRANCH(1); EMIT("Check credits or billing in Nous Portal, then retry."); return; }  /* B7 */
        WCUP_BRANCH(0);
        EMIT("Subscription credits are exhausted. Top up/renew credits, then retry.");
        return;
    }
    WCUP_BRANCH(0);
    if (strcmp(code, "temporarily_unavailable") == 0) { /* B8 */
        WCUP_BRANCH(1);
        char rbuf[512];
        snprintf(rbuf, sizeof rbuf, "%s Please retry in a few seconds.", m);
        EMIT(rbuf);
        return;
    }
    WCUP_BRANCH(0);
    EMIT(m);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_auth_error <code|prov|relogin|msg>...\n", argv[0]); return 2; }
    for (int k = 2; k < argc; k++) {
        char *s = argv[k];
        char *f1 = strchr(s, '|');
        char *f2 = f1 ? strchr(f1 + 1, '|') : NULL;
        char *f3 = f2 ? strchr(f2 + 1, '|') : NULL;
        if (!f3) { fprintf(stderr, "bad input: %s\n", s); return 2; }
        *f1 = '\0'; *f2 = '\0'; *f3 = '\0';
        const char *code = strcmp(s, "-") == 0 ? NULL : s;
        const char *provider = f1 + 1;
        int relogin = atoi(f2 + 1);
        const char *msg = f3 + 1;
        format_auth_error(code, provider, relogin, msg);
    }
    return 0;
}
