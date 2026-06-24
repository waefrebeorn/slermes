/*
 * agent_init.c — Agent initialization extracted from AIAgent.__init__.
 *
 * Port of Python agent/agent_init.py (1739 lines, 7 functions).
 * 5/5 portable functions are ported across agent_init.c + provider_custom.c.
 * 2 functions are N/A (Python-only runtime coupling).
 *
 * Ported functions:
 *   _build_codex_gpt55_autoraise_notice     — this file (below)
 *   _normalized_custom_base_url             — this file + provider_custom.c:299
 *   _custom_provider_model_matches          — provider_custom.c:315
 *   _custom_provider_extra_body_for_agent   — provider_custom.c:330
 *   _merge_custom_provider_extra_body       — provider_custom.c:381
 *   _ra                                     — N/A (Python import-time module reference)
 *   init_agent                              — N/A (Python 1400-line attribute init; C uses agent_state_t directly)
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <math.h>

/* Port of Python: _build_codex_gpt55_autoraise_notice
 * Builds the one-time notice shown when Codex gpt-5.5 raises compaction.
 * autoraise is a JSON object with "from" and "to" float fields (ratios 0.0-1.0).
 * Returns a malloc'd string that caller must free.
 */
char *agent_build_codex_gpt55_autoraise_notice(const json_t *autoraise) {
    if (!autoraise || autoraise->type != JSON_OBJECT) return NULL;

    double from_ratio = json_get_num(autoraise, "from", 0.0);
    double to_ratio = json_get_num(autoraise, "to", 0.0);

    int from_pct = (int)round(from_ratio * 100.0);
    int to_pct = (int)round(to_ratio * 100.0);

    /* Build formatted message */
    char *msg = NULL;
    int len = asprintf(&msg,
        "ℹ Codex gpt-5.5 caps context at 272K, so auto-compaction was raised "
        "to %d%% (from %d%%) to use more of the window before "
        "summarizing.\n"
        "  Opt back out: hermes config set compression.codex_gpt55_autoraise false",
        to_pct, from_pct);

    if (len < 0) {
        return NULL;
    }
    return msg;
}
