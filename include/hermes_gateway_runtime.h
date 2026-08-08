/* hermes_gateway_runtime.h — Gateway runtime features.
 * Agent cache, prefill messages, provider routing, service tier,
 * notification mode, stop/drain, systemd, goal management, streaming.
 */

#ifndef HERMES_GATEWAY_RUNTIME_H
#define HERMES_GATEWAY_RUNTIME_H

#include "hermes_core_types.h"
#include <stdbool.h>

/* ═══ 1. Agent Cache ═══ */
void gw_agent_cache_init(void);
agent_state_t *gw_agent_cache_get(const char *session_key);
bool gw_agent_cache_put(const char *session_key, const agent_state_t *agent);
void gw_agent_cache_remove(const char *session_key);
int  gw_agent_cache_sweep_idle(void);
int  gw_agent_cache_enforce_cap(void);
int  gw_agent_cache_size(void);

/* ═══ 2. Prefill / Ephemeral System Prompt ═══ */
void gw_load_prefill_messages(void);
void gw_load_ephemeral_system_prompt(void);
int  gw_prefill_count(void);
const char *gw_prefill_role(int idx);
const char *gw_prefill_content(int idx);
const char *gw_ephemeral_system_prompt(void);

/* ═══ 3. Provider Routing + Fallback ═══ */
void gw_load_provider_routing(void);
int  gw_fallback_count(void);
const char *gw_fallback_provider(int idx);
bool gw_fallback_enabled(void);

/* ═══ 4. Service Tier ═══ */
void gw_load_service_tier(void);
const char *gw_service_tier(void);

/* ═══ 5. Notification Mode ═══ */
void gw_load_notification_mode(void);
const char *gw_notification_mode(void);
bool gw_notify_silent(void);
bool gw_notify_mentions_only(void);

/* ═══ 6. Runtime Init ═══ */
void gw_runtime_init(void);

/* ═══ 7. Stop/Drain ═══ */
/* Port of gateway/run.py:_drain_active_agents — wait (0.1s polls) up to
 * timeout_sec for in-flight turns / cron jobs / queued messages to finish.
 * Returns true iff the deadline passed with work still active. */
bool gw_drain_active_agents(int timeout_sec);
void gw_notify_sessions_shutdown(const char *reason);

/* Graceful-shutdown request seam (port of GatewayRunner.stop()).
 * Signal-safe: may be called from a signal handler. The main thread
 * (which blocks in gw_wait_for_shutdown_request) is woken via a
 * self-pipe so the cond_wait returns and the drain sequence runs. */
void gw_request_shutdown(const char *reason);   /* signal-safe */
bool gw_shutdown_requested(void);
const char *gw_shutdown_reason(void);
void gw_wait_for_shutdown_request(void);        /* blocks main thread */
void gw_shutdown_seam_init(void);               /* bootstrap self-pipe */

/* ═══ 8. Systemd Integration ═══ */
void gw_systemd_notify(const char *state);
bool gw_under_systemd(void);

/* ═══ 9. Detached Restart ═══ */
bool gw_detached_restart(void);
bool gw_handoff_save_state(void);
bool gw_handoff_restore_state(void);

/* ═══ 10. Goal Management ═══ */
void gw_goal_record_turn(const char *session_key, int turn_count);
int  gw_goal_max_turns(const char *session_key);
void gw_goal_clear(const char *session_key);

/* ═══ 11. Streaming Dispatch ═══ */
typedef void (*gw_stream_callback_t)(const char *token, void *userdata);
void gw_stream_set_callback(gw_stream_callback_t cb, void *userdata);
bool gw_stream_is_active(void);
void gw_stream_send(const char *text);
void gw_stream_end(void);

#endif /* HERMES_GATEWAY_RUNTIME_H */
