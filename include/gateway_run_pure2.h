/*
 * gateway_run_pure2.h — declaration surface for the second batch of pure
 * gateway/run.py helpers ported in src/gateway/run_pure2.c.
 *
 * Deterministic data/string/config transforms. No network or async coupling.
 * Oracle-verified against the canonical Python where possible.
 */

#ifndef GATEWAY_RUN_PURE2_H
#define GATEWAY_RUN_PURE2_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_core_types.h"   /* hermes_config_t */
#include "hermes_json.h"         /* json_node_t */

#ifdef __cplusplus
extern "C" {
#endif

/* run.py _reconnect_backoff — 30*2^(n-1) capped at 300. */
/* PoP: gw_reconnect_backoff @ gateway/run.py:_reconnect_backoff */
int gw_reconnect_backoff(int attempt);

/* run.py _auto_continue_freshness_window */
/* PoP: gw_auto_continue_freshness_window @ gateway/run.py:_auto_continue_freshness_window */
double gw_auto_continue_freshness_window(void);

/* run.py _ensure_windows_gateway_venv_imports (no-op on non-Windows) */
/* PoP: gw_ensure_windows_gateway_venv_imports @ gateway/run.py:_ensure_windows_gateway_venv_imports */
void gw_ensure_windows_gateway_venv_imports(void);

/* run.py _gateway_loop_exception_handler — transient-class predicate. */
/* PoP: gw_gateway_loop_exception_handler @ gateway/run.py:_gateway_loop_exception_handler */
bool gw_gateway_loop_exception_handler_is_transient(const char *exc_name);

/* run.py _ensure_ssl_certs */
/* PoP: gw_ensure_ssl_certs @ gateway/run.py:_ensure_ssl_certs */
void gw_ensure_ssl_certs(void);

/* run.py _restart_notification_pending */
/* PoP: gw_restart_notification_pending @ gateway/run.py:_restart_notification_pending */
bool gw_restart_notification_pending(void);

/* run.py _planned_restart_notification_path */
/* PoP: gw_planned_restart_notification_path @ gateway/run.py:_planned_restart_notification_path */
void gw_planned_restart_notification_path(char *out, size_t cap);

/* run.py _planned_restart_notification_pending */
/* PoP: gw_planned_restart_notification_pending @ gateway/run.py:_planned_restart_notification_pending */
bool gw_planned_restart_notification_pending(void);

/* run.py _clear_planned_restart_notification */
/* PoP: gw_clear_planned_restart_notification @ gateway/run.py:_clear_planned_restart_notification */
void gw_clear_planned_restart_notification(void);

/* run.py _platform_has_bot_credential */
/* PoP: gw_platform_has_bot_credential @ gateway/run.py:_platform_has_bot_credential */
bool gw_platform_has_bot_credential(const char *platform,
                                     const char *token,
                                     const char *api_key);

/* run.py _resolve_hermes_bin — returns malloc'd "hermes" or NULL. */
/* PoP: gw_resolve_hermes_bin @ gateway/run.py:_resolve_hermes_bin */
char *gw_resolve_hermes_bin(void);

/* run.py _load_gateway_config — returns heap hermes_config_t* (caller frees). */
/* PoP: gw_load_gateway_config @ gateway/run.py:_load_gateway_config */
hermes_config_t *gw_load_gateway_config(void);

/* run.py _load_gateway_runtime_config */
/* PoP: gw_load_gateway_runtime_config @ gateway/run.py:_load_gateway_runtime_config */
hermes_config_t *gw_load_gateway_runtime_config(void);

/* run.py _resolve_gateway_model */
/* PoP: gw_resolve_gateway_model @ gateway/run.py:_resolve_gateway_model */
void gw_resolve_gateway_model(const hermes_config_t *cfg, char *out, size_t cap);

/* run.py _channel_override_lookup_keys — fills out_keys[cap] malloc'd strings. */
/* PoP: gw_channel_override_lookup_keys @ gateway/run.py:_channel_override_lookup_keys */
int gw_channel_override_lookup_keys(const char *chat_id, const char *thread_id,
                                    const char *parent_id, char **out_keys, int cap);

/* run.py _build_gateway_agent_history */
/* PoP: gw_build_gateway_agent_history @ gateway/run.py:_build_gateway_agent_history */
int gw_build_gateway_agent_history(const char *history_json,
                                   bool inject_timestamps,
                                   char ***out_history, int *out_n,
                                   char **out_observed);

/* run.py _wrap_current_message_with_observed_context — malloc'd string. */
/* PoP: gw_wrap_current_message_with_observed_context @ gateway/run.py:_wrap_current_message_with_observed_context */
char *gw_wrap_current_message_with_observed_context(const char *message,
                                                     const char *observed_context);

/* run.py _collect_auto_append_media_tags — malloc'd newline-joined MEDIA tags. */
/* PoP: gw_collect_auto_append_media_tags @ gateway/run.py:_collect_auto_append_media_tags */
char *gw_collect_auto_append_media_tags(const char *messages_json,
                                        int history_offset,
                                        const char *history_media_paths_json,
                                        bool *out_has_voice);

/* run.py _collect_history_media_paths — malloc'd JSON array string. */
/* PoP: gw_collect_history_media_paths @ gateway/run.py:_collect_history_media_paths */
char *gw_collect_history_media_paths(const char *agent_history_json);

/* run.py _is_gateway_hidden_reasoning_incomplete_turn */
/* PoP: gw_is_gateway_hidden_reasoning_incomplete_turn @ gateway/run.py:_is_gateway_hidden_reasoning_incomplete_turn */
bool gw_is_gateway_hidden_reasoning_incomplete_turn(const json_node_t *agent_result);

/* run.py _should_clear_resume_pending_after_turn */
/* PoP: gw_should_clear_resume_pending_after_turn @ gateway/run.py:_should_clear_resume_pending_after_turn */
bool gw_should_clear_resume_pending_after_turn(const json_node_t *agent_result);

/* run.py _preserve_queued_followup_history_offset — malloc'd JSON or NULL. */
/* PoP: gw_preserve_queued_followup_history_offset @ gateway/run.py:_preserve_queued_followup_history_offset */
char *gw_preserve_queued_followup_history_offset(const char *current_json,
                                                  const char *followup_json);

/* run.py _bridge_max_turns_from_config */
/* PoP: gw_bridge_max_turns_from_config @ gateway/run.py:_bridge_max_turns_from_config */
void gw_bridge_max_turns_from_config(void);

/* run.py _reload_runtime_env_preserving_config_authority */
/* PoP: gw_reload_runtime_env_preserving_config_authority @ gateway/run.py:_reload_runtime_env_preserving_config_authority */
void gw_reload_runtime_env_preserving_config_authority(void);

/* run.py _current_max_iterations */
/* PoP: gw_current_max_iterations @ gateway/run.py:_current_max_iterations */
int gw_current_max_iterations(void);

/* run.py load_gateway_config_for_runner */
/* PoP: gw_load_gateway_config_for_runner @ gateway/run.py:load_gateway_config_for_runner */
hermes_config_t *gw_load_gateway_config_for_runner(void);

/* run.py _adapter_disconnect_timeout_secs — env-tunable disconnect timeout. */
/* PoP: gw_adapter_disconnect_timeout_secs @ gateway/run.py:_adapter_disconnect_timeout_secs */
double gw_adapter_disconnect_timeout_secs(void);

/* run.py _platform_connect_timeout_secs — env-tunable connect timeout. */
/* PoP: gw_platform_connect_timeout_secs @ gateway/run.py:_platform_connect_timeout_secs */
double gw_platform_connect_timeout_secs(void);

/* run.py _is_telegram_topic_root_lobby — pass DB-resolved topic_mode_enabled. */
/* PoP: gw_is_telegram_topic_root_lobby @ gateway/run.py:_is_telegram_topic_root_lobby */
bool gw_is_telegram_topic_root_lobby(const char *platform,
                                     const char *chat_type,
                                     const char *thread_id,
                                     bool topic_mode_enabled);

/* run.py _is_telegram_topic_lane — pass DB-resolved topic_mode_enabled. */
/* PoP: gw_is_telegram_topic_lane @ gateway/run.py:_is_telegram_topic_lane */
bool gw_is_telegram_topic_lane(const char *platform,
                               const char *chat_type,
                               const char *thread_id,
                               bool topic_mode_enabled);

/* run.py _telegram_topic_new_header — static header string or NULL. */
/* PoP: gw_telegram_topic_new_header @ gateway/run.py:_telegram_topic_new_header */
const char *gw_telegram_topic_new_header(const char *platform,
                                         const char *chat_type,
                                         const char *thread_id,
                                         bool topic_mode_enabled);

/* run.py _is_telegram_dm_topic_target — pass adapter-resolved dict flag. */
/* PoP: gw_is_telegram_dm_topic_target @ gateway/run.py:_is_telegram_dm_topic_target */
bool gw_is_telegram_dm_topic_target(const char *platform,
                                    const char *chat_id,
                                    const char *thread_id,
                                    const char *chat_type,
                                    bool has_dm_topic_info);

/* run.py _adapter_credential_fingerprint — salted sha256[:16] of token. */
/* PoP: gw_adapter_credential_fingerprint @ gateway/run.py:_adapter_credential_fingerprint */
char *gw_adapter_credential_fingerprint(const char *token);

/* run.py _empty_honcho_cache_busting_config — JSON object, all-null values. */
/* PoP: gw_empty_honcho_cache_busting_config @ gateway/run.py:_empty_honcho_cache_busting_config */
char *gw_empty_honcho_cache_busting_config(void);

/* run.py _get_proxy_url — GATEWAY_PROXY_URL env else config value, rstrip "/". */
/* PoP: gw_get_proxy_url @ gateway/run.py:_get_proxy_url */
char *gw_get_proxy_url(const char *config_proxy_url);

/* run.py _thread_metadata_for_target — malloc'd json_t or NULL. */
/* PoP: gw_thread_metadata_for_target @ gateway/run.py:_thread_metadata_for_target */
json_t *gw_thread_metadata_for_target(const char *thread_id,
                                      bool is_dm_topic,
                                      const char *reply_to_message_id);

/* run.py _checkpoint_agent_kwargs — malloc'd json_t of AIAgent ctor args. */
/* PoP: gw_checkpoint_agent_kwargs @ gateway/run.py:_checkpoint_agent_kwargs */
json_t *gw_checkpoint_agent_kwargs(bool cp_enabled,
                                   int max_snapshots,
                                   int max_total_size_mb,
                                   int max_file_size_mb);

/* run.py _load_voice_modes — filtered chat_id->mode object from raw JSON. */
/* PoP: gw_load_voice_modes @ gateway/run.py:_load_voice_modes */
json_t *gw_load_voice_modes(const char *voice_modes_json);

/* run.py _save_voice_modes — serialize voice-mode object (indent=2). */
/* PoP: gw_save_voice_modes @ gateway/run.py:_save_voice_modes */
char *gw_save_voice_modes(const json_t *voice_mode);

/* run.py _is_discord_auto_thread_lane — Discord auto-created thread predicate. */
/* PoP: gw_is_discord_auto_thread_lane @ gateway/run.py:_is_discord_auto_thread_lane */
bool gw_is_discord_auto_thread_lane(const char *platform,
                                    const char *chat_type,
                                    const char *thread_id,
                                    bool auto_thread_created,
                                    const char *auto_thread_initial_name);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_RUN_PURE2_H */
