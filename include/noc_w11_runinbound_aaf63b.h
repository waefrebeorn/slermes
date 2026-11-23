#ifndef NOC_W11_RUNINBOUND_AAF63B_H
#define NOC_W11_RUNINBOUND_AAF63B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_runinbound_aaf63b_rehome @ gateway/run_inbound_media.py:rehome_inbound_media
 * Faithful port of the pure re-homing decisions. Returns a NEW object with
 * keys "media_urls" (array), "text" (string or null) and
 * "_gateway_pending_stt_text" (string or null). `active_home` / `launch_home`
 * stand in for hermes_constants' getters and `agent_map` maps a path string to
 * its agent-visible path (to_agent_visible_cache_path). Filesystem moves are
 * assumed to succeed (all sources are files), so the string decisions match. */
struct json_t *w11_runinbound_aaf63b_rehome(const struct json_t *event,
                                             const char *active_home,
                                             const char *launch_home,
                                             const struct json_t *agent_map);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_RUNINBOUND_AAF63B_H */
