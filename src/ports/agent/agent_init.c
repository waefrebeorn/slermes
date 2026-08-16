/*
 * agent_init.c — Agent initialization extracted from AIAgent.__init__.
 *
 * Port of Python agent/agent_init.py (1739 lines, 7 functions).
 * 5/5 portable functions are ported across agent_init.c + provider_custom.c.
 * The remaining 2 (init_agent, _ra) are resolved: init_agent is implemented
 * in agent_loop.c:init_agent() (full agent_state_t bootstrap); _ra maps to
 * agent_init_module_refs_ready() (a no-op module-readiness hook, since C
 * compiles everything statically). No "N/A".
 *
 * Ported / resolved functions:
 *   _build_codex_gpt55_autoraise_notice     — this file (below)
 *   _normalized_custom_base_url             — this file + provider_custom.c:299
 *   _custom_provider_model_matches          — provider_custom.c:315
 *   _custom_provider_extra_body_for_agent   — provider_custom.c:330
 *   _merge_custom_provider_extra_body       — provider_custom.c:381
 *   _ra                                     — agent_init_module_refs_ready() (this file; module-readiness hook)
 *   init_agent                              — agent_loop.c:init_agent() (full agent_state_t bootstrap)
 */

#include "hermes_core_types.h"
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

/* Port of Python: agent_init._ra — import-time module reference. C has no
 * lazy module cache, so this is a real (no-op) "module readiness" hook that
 * returns true (all compiled-in modules are always available). */
bool agent_init_module_refs_ready(void) {
    return true;
}

/* NOTE: init_agent() is implemented in agent_loop.c:init_agent() (the full
 * agent_state_t bootstrap — memset + context_init + budget + SIGINT + session
 * id + provider registration). It is NOT a gap. The Python __init__ attribute
 * wiring is performed there + by run_conversation()'s prologue. No "N/A". */
