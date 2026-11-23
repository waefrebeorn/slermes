/*
 * prepared_compaction.c — C home for upstream Hermes agent/prepared_compaction.py.
 *
 * Faithful C11 ports of the module's pure "is a prepared pass blocked?" decision
 * helpers.  Platform oneshot probing, monotonic time and all agent/compressor
 * attributes are injected through the args object (the oracle stubs the same
 * seams), so the decision logic itself is exercised exactly as written.
 *
 * PoP: w11_preparedco_e759f4_blocked_for_platform @ agent/prepared_compaction.py:_blocked_for_platform
 * PoP: w11_preparedco_e759f4_blocked @ agent/prepared_compaction.py:PreparedCompaction._blocked
 */
#include "noc_w11_preparedco_e759f4.h"

#include <string.h>

#define PREPARE_BAND_RATIO 0.12

static int platform_blocks(json_t *agent, const char *origin, int oneshot) {
    if (strcmp(origin, "turn-end")) return 0;
    json_t *platform = json_obj_get(agent, "platform");
    if (json_is_string(platform)) {
        const char *p = json_string_value(platform);
        if (p && (!strcmp(p, "cron") || !strcmp(p, "subagent"))) return 1;
    }
    if (json_get_num(agent, "_delegate_depth", 0) > 0) return 1;
    return oneshot ? 1 : 0;   /* injected agent_is_oneshot(agent) */
}

/* PoP: w11_preparedco_e759f4_blocked_for_platform @ agent/prepared_compaction.py:_blocked_for_platform */
json_t *w11_preparedco_e759f4_blocked_for_platform(json_t *a) {
    const char *origin = json_get_str(a, "origin", "");
    int oneshot = json_get_bool(a, "oneshot", false) ? 1 : 0;
    return json_bool(platform_blocks(a, origin, oneshot) ? true : false);
}

/* PoP: w11_preparedco_e759f4_blocked @ agent/prepared_compaction.py:PreparedCompaction._blocked */
json_t *w11_preparedco_e759f4_blocked(json_t *a) {
    json_t *agent = json_obj_get(a, "agent");
    json_t *comp = json_obj_get(a, "compressor");
    const char *origin = json_get_str(a, "origin", "");

    if (!json_get_bool(agent, "compression_enabled", false)) return json_bool(true);
    if (json_get_bool(agent, "_persist_disabled", false)) return json_bool(true);
    const char *api_mode = json_get_str(agent, "api_mode", "");
    if (api_mode && !strcmp(api_mode, "codex_app_server")) return json_bool(true);
    if (json_get_bool(agent, "codex_responses_native_compaction", false)) return json_bool(true);
    if (json_get_bool(comp, "_micro_compact_enabled", false)) return json_bool(true);
    if (json_get_bool(a, "needs_provider_context", false)) return json_bool(true);

    int oneshot = json_get_bool(a, "oneshot", false) ? 1 : 0;
    if (platform_blocks(agent, origin, oneshot)) return json_bool(true);

    if (json_get_bool(comp, "awaiting_real_usage_after_compression", false)) return json_bool(true);
    if (json_get_num(comp, "_fallback_compression_streak", 0) >= 2) return json_bool(true);

    double tokens = json_get_num(comp, "last_prompt_tokens", 0);
    double threshold = json_get_num(comp, "threshold_tokens", 0);
    long long band_floor = (long long)(threshold * (1.0 - PREPARE_BAND_RATIO));
    if (tokens <= 0 || threshold <= 0 || tokens < (double)band_floor) return json_bool(true);

    double now = json_get_num(a, "now", 0);
    double cooldown = json_get_num(comp, "_summary_failure_cooldown_until", 0);
    double retry = json_get_num(a, "retry_after", 0);
    if (now < cooldown || now < retry) return json_bool(true);

    return json_bool(false);
}
