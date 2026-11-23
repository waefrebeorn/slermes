/*
 * telegram_draft_budget.c — C home for plugins/platforms/telegram/telegram_draft_budget.py.
 * PoP: tgdraft_claim_ai_action @ plugins/platforms/telegram/telegram_draft_budget.py:claim_ai_action
 *
 * The live function keeps a per-chat deque of call timestamps on the adapter and
 * reads loop time from asyncio.  The _ex seam takes both hidden inputs explicitly:
 * `now` and the current call timestamps.  It returns the skip delay (0.0 when a
 * slot was claimed) and writes the resulting timestamp deque to `new_calls_out`.
 * Windows: (5.0 s, 20 calls), (30.0 s, 40 calls); _LONGEST = 30.0 s.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#define TG_LONGEST 30.0

double tgdraft_claim_ai_action_ex(double now, const double *calls, size_t n, json_t **new_calls_out) {
    size_t start = 0;
    while (start < n && now - calls[start] >= TG_LONGEST) start++;  /* popleft stale */
    size_t len = n - start;
    double delay = 0.0;
    if (len >= 20) {
        double v = calls[start + len - 20] + 5.0 - now;
        if (v > delay) delay = v;
    }
    if (len >= 40) {
        double v = calls[start + len - 40] + 30.0 - now;
        if (v > delay) delay = v;
    }
    json_t *result = json_array();
    if (delay > 0.0) {
        for (size_t i = start; i < n; i++) json_append(result, json_number(calls[i]));
    } else {
        for (size_t i = start; i < n; i++) json_append(result, json_number(calls[i]));
        json_append(result, json_number(now));  /* append(now) on a claimed slot */
    }
    if (new_calls_out) *new_calls_out = result; else json_free(result);
    return delay;
}
