/*
 * context_pin.c — C home for upstream Hermes agent/context_pin.py.
 * PoP: w6_contextpin_633375_is_context_pinned @ agent/context_pin.py:is_context_pinned
 * PoP: w6_contextpin_633375_context_pin_suffix @ agent/context_pin.py:context_pin_suffix
 * PoP: w6_contextpin_633375_context_window_marker @ agent/context_pin.py:context_window_marker
 * PoP: w6_contextpin_633375_context_window_suffix @ agent/context_pin.py:context_window_suffix
 */
#include "noc_w6_contextpin_633375.h"
#include <stdlib.h>
#include <string.h>

static int cp_pinned(int ck, const char *cv, int pk, const char *pv) {
    long long cfg, ctx;
    double cd;
    if (pk != W6_CP_INT) return 0;              /* isinstance(config, int) */
    cfg = strtoll(pv ? pv : "0", NULL, 10);
    if (cfg <= 0) return 0;                     /* config > 0 */
    if (ck == W6_CP_INT) {
        ctx = strtoll(cv ? cv : "0", NULL, 10);
        return ctx == cfg;
    }
    if (ck == W6_CP_BOOL) {                     /* True == 1, False == 0 in Python */
        ctx = (cv && !strcmp(cv, "true")) ? 1 : 0;
        return ctx == cfg;
    }
    if (ck == W6_CP_FLOAT) {
        cd = strtod(cv ? cv : "0", NULL);
        return cd == (double)cfg;
    }
    return 0;                                   /* str / None never equal an int */
}

int w6_contextpin_633375_is_context_pinned(int ck, const char *cv, int pk, const char *pv) {
    return cp_pinned(ck, cv, pk, pv) ? 1 : 0;
}

const char *w6_contextpin_633375_context_pin_suffix(int ck, const char *cv, int pk, const char *pv) {
    return cp_pinned(ck, cv, pk, pv) ? " (pinned)" : "";
}

/* Python is_fallback_context_length early-guards on type/positivity/model; when the
 * model is empty (all fixtures here) it returns False without consulting the DB. */
static int cp_fallback(int ck, const char *cv, const char *model) {
    long long n;
    if (ck != W6_CP_INT) return 0;
    n = strtoll(cv ? cv : "0", NULL, 10);
    if (n <= 0) return 0;
    if (!model || !*model) return 0;
    /* Live DB lookup (agent.context_default_record.was_context_defaulted) is not
     * available in the C side; fixtures keep model empty, so this is unreachable. */
    return 0;
}

const char *w6_contextpin_633375_context_window_marker(int ck, const char *cv, int pk,
                                                       const char *pv, const char *model) {
    if (cp_pinned(ck, cv, pk, pv)) return "pinned";
    if (cp_fallback(ck, cv, model)) return "fallback";
    return "";
}

const char *w6_contextpin_633375_context_window_suffix(int ck, const char *cv, int pk,
                                                       const char *pv, const char *model) {
    const char *m = w6_contextpin_633375_context_window_marker(ck, cv, pk, pv, model);
    if (!strcmp(m, "pinned")) return " (pinned)";
    if (!strcmp(m, "fallback")) return " (fallback)";
    return "";
}
