/*
 * noc_w26_agentinitc_084bfa.h - C surface for upstream
 * agent/agent_init_compression_notice.py (gate w26_agentinitc_084bfa).
 *
 * The init-time compression banner and the once-per-profile Codex gpt-5.x
 * autoraise notice. The banner text, the notice's identity key and the
 * once-per-state decision are pure functions of the autoraise dict, the active
 * engine's threshold numbers and the profile marker state; the marker file,
 * the agent and its console are seams.
 */
#ifndef NOC_W26_AGENTINITC_084BFA_H
#define NOC_W26_AGENTINITC_084BFA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"autoraise": {...}, "context_length": <int|null>} -> the notice text. */
json_t *w26_agentinitc_084bfa_notice(const json_t *req);

/* req: {"autoraise": {...}} -> "<model>:<from%>:<to%>", the marker identity. */
json_t *w26_agentinitc_084bfa_state(const json_t *req);

/* req: {"autoraise", "enabled", "notice_enabled", "seen", "quiet_mode",
 *       "context_length", "threshold_percent", "threshold_tokens", "eff_cap",
 *       "tokens_cap"} -> {"warning": <str|null>, "line": <str|null>}
 * The printed context-limit line and the replayed notice the init leaves behind. */
json_t *w26_agentinitc_084bfa_emit(const json_t *req);

/* The banner's fixed numbers and opt-out text. */
json_t *w26_agentinitc_084bfa_constants(void);

#ifdef __cplusplus
}
#endif
#endif
