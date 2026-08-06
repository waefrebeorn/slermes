/* gateway_run_helpers.h — Pure helpers ported from gateway/run.py
 *
 * Opaque API: all functions are stateless pure transformations.
 * No gateway state is accessed — these are leaf helpers.
 *
 * Port of: gateway/run.py module-level functions
 * C11, minimal includes, no god headers.
 */
#ifndef GATEWAY_RUN_HELPERS_H
#define GATEWAY_RUN_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
/* Opaque GatewayRunner handle (defined in hermes_gateway_runner.h). */
typedef struct GatewayRunner GatewayRunner;

#ifdef __cplusplus
extern "C" {
#endif

/* ── Platform helpers ───────────────────────────────────────────── */

/* Normalise a platform value string (port of _gateway_platform_value).
   Returns a lower-cased, stripped copy in out[0..out_size-1]. */
void gw_platform_value(const char *platform, char *out, size_t out_size);

/* True when platform is a raw-text surface (CLI, API, webhook).
   Port of _gateway_surface_passes_raw_text. */
bool gw_surface_passes_raw_text(const char *platform);

/* ── Network / error helpers ─────────────────────────────────────── */

/* True when exc_class_name is a transient network error class.
   Port of _is_transient_network_error — checks class name against set. */
bool gw_is_transient_network_error(const char *exc_class_name);

/* Port of _gateway_provider_error_reply: map text to user-safe reply. */
void gw_provider_error_reply(const char *text, char *out, size_t out_size);

/* True when text looks like a provider error envelope.
   Port of _looks_like_gateway_provider_error. */
bool gw_looks_like_provider_error(const char *text);

/* Port of _sanitize_gateway_final_response: redact + replace errors. */
void gw_sanitize_final_response(const char *platform, const char *text,
                                char *out, size_t out_size);

/* Port of _prepare_gateway_status_message: filter/sanitize.
   Returns 0 when message should be suppressed, 1 when out is filled. */
int gw_prepare_status_message(const char *platform, const char *event_type,
                              const char *message, char *out, size_t out_size);

/* ── Secret redaction ────────────────────────────────────────────── */

/* Port of _redact_gateway_user_facing_secrets */
void gw_redact_secrets(const char *text, char *out, size_t out_size);

/* Port of _redact_approval_command */
void gw_redact_approval_command(const char *cmd, char *out, size_t out_size);

/* ── Formatting helpers ──────────────────────────────────────────── */

/* Port of _format_exec_approval_fallback. */
void gw_format_exec_approval_fallback(const char *command,
                                      const char *description,
                                      const char *command_prefix,
                                      bool allow_permanent,
                                      bool allow_session,
                                      bool smart_denied,
                                      char *out, size_t out_size);

/* Port of render_notice_line — extract notice text. */
void gw_render_notice_line(const char *notice_text, char *out, size_t out_size);

/* Port of _format_duration — seconds to "M:SS" or "H:MM:SS". */
void gw_format_duration(double seconds, char *out, size_t out_size);

/* ── Thread / progress helpers ───────────────────────────────────── */

/* Port of _resolve_progress_thread_id. */
void gw_resolve_progress_thread_id(const char *platform,
                                   const char *source_thread_id,
                                   const char *event_message_id,
                                   char *out, size_t out_size);

/* ── Display config helpers ──────────────────────────────────────── */

/* Port of _has_platform_display_override — deeply inspect JSON-like config.
   user_config is a JSON string; function uses libjson for traversal. */
bool gw_has_display_override(const char *user_config_json,
                             const char *platform_key,
                             const char *setting);

/* Port of _resolve_gateway_display_bool. Returns true/false default. */
bool gw_resolve_display_bool(const char *user_config_json,
                              const char *platform_key,
                              const char *setting,
                              bool default_val,
                              const char *platform,
                              const char *require_override_platforms_json);

/* ── Timestamp / freshness helpers ───────────────────────────────── */

/* Port of _coerce_gateway_timestamp — best-effort to epoch seconds.
   Returns -1.0 on failure/unparseable.
   Handles: datetime (epoch-magnitude), int/float (ms vs s),
   ISO-8601 strings, numeric strings. */
double gw_coerce_timestamp(double value, int is_ms,
                           const char *iso_string,
                           int is_iso_string);

/* Port of _float_env — read env var as float. */
double gw_float_env(const char *name, double default_val);

/* Port of _is_fresh_gateway_interruption. */
bool gw_is_fresh_interruption(double timestamp, double now, double window_secs);

/* Port of build_resume_recovery_note — system prompt builder. */
void gw_build_resume_recovery_note(const char *reason, const char *message,
                                    bool interactive,
                                    char *out, size_t out_size);

/* ── Transcript replay helpers ───────────────────────────────────── */

/* Port of _uses_telegram_observed_group_context. */
bool gw_uses_observed_group_context(const char *channel_prompt);

/* Port of _message_timestamps_enabled — deep config lookup. */
bool gw_message_timestamps_enabled(const char *user_config_json);

/* Port of _last_transcript_timestamp — find last usable row's timestamp.
   Returns -1.0 when no usable timestamp found. */
double gw_last_transcript_timestamp(const char *history_json);

/* Port of _is_auto_continue_noise. */
bool gw_is_auto_continue_noise(const char *content);

/* Port of _strip_auto_continue_noise. */
void gw_strip_auto_continue_noise(const char *content,
                                   char *out, size_t out_size);

/* ── Media helpers ───────────────────────────────────────────────── */

/* Port of _format_duration (reused above). */
/* Port of _event_media_type_at — returns mime type string. */
void gw_event_media_type_at(const char *event_json, int index,
                             char *out, size_t out_size);

bool gw_event_media_is_image(const char *event_json, int index);
bool gw_event_media_is_audio(const char *event_json, int index);
bool gw_event_media_is_video(const char *event_json, int index);
bool gw_event_media_is_stt_input(const char *event_json, int index);

/* Port of _build_media_placeholder. */
void gw_build_media_placeholder(const char *event_json,
                                 char *out, size_t out_size);

/* Port of _build_document_context_note. */
void gw_build_document_context_note(const char *display_name,
                                     const char *agent_path,
                                     const char *mtype,
                                     char *out, size_t out_size);

/* ── Control / misc helpers ──────────────────────────────────────── */

/* Port of _is_control_interrupt_message. */
bool gw_is_control_interrupt_message(const char *message);

/* Port of _skill_slug_from_frontmatter — parse YAML frontmatter.
   Returns slug in slug_out, name in name_out. Both are NULL if missing. */
void gw_skill_slug_from_frontmatter(const char *skill_md_content,
                                     char *slug_out, size_t slug_size,
                                     char *name_out, size_t name_size);

/* Port of _check_unavailable_skill — check command_name against known skills.
   Returns static string or NULL. */
const char *gw_check_unavailable_skill(const char *command_name);

/* Port of _platform_config_key */
void gw_platform_config_key(const char *platform, char *out, size_t out_size);

/* Port of _teams_pipeline_plugin_enabled */
bool gw_teams_pipeline_plugin_enabled(void);

/* Port of _gateway_config_home — path builder. */
void gw_gateway_config_home(char *out, size_t out_size);

/* Port of _parse_session_key — parse "platform:chat_id" format.
   Returns platform in platform_out, id in id_out. Both empty on failure. */
void gw_parse_session_key(const char *session_key,
                           char *platform_out, size_t platform_size,
                           char *id_out, size_t id_size);

/* ── Remaining gateway/run.py REAL_GAP ports ──────────────────── */

/* _record_hygiene_cooldown — persist compression cooldown per session */
void gw_record_hygiene_cooldown(GatewayRunner *self, const char *session_key,
                                double cooldown_seconds);

/* _seed_hygiene_system_prompt — seed system prompt for hygiene compression */
bool gw_seed_hygiene_system_prompt(GatewayRunner *self, const char *session_key,
                                    const char *system_prompt);

/* _startup_restore_drain_timeout_secs — env var for drain timeout */
double gw_startup_restore_drain_timeout_secs(void);

/* _stamp_hygiene_compression_provenance — log provenance stamp */
void gw_stamp_hygiene_compression_provenance(GatewayRunner *self,
                                              const char *session_key,
                                              const char *desc);

/* _reap_gateway_turn_processes — kill stale gateway turn processes */
int gw_reap_gateway_turn_processes(GatewayRunner *self, const char *session_key,
                                    const char *source, bool is_still_current);

/* _abandon_timed_out_gateway_turn — interrupt a timed-out running agent */
bool gw_abandon_timed_out_gateway_turn(GatewayRunner *self, const char *session_key,
                                        const char *source, bool is_still_current);

/* _watch_gateway_turn_inactivity — watchdog thread for idle agents */
void *gw_watch_gateway_turn_inactivity(void *arg);

/* progress_callback hook */
void gw_progress_callback(GatewayRunner *self, const char *event_type,
                            const char *tool_name, const char *preview);

/* send_progress_messages — flush pending progress to session adapter */
void gw_send_progress_messages(GatewayRunner *self, const char *session_key);

/* voice_ack_callback — voice ack for voice channels */
void gw_voice_ack_callback(GatewayRunner *self, const char *session_key,
                             const char *call_id, const char *tool_name);

/* _step_callback_sync — emit agent:step hook */
void gw_step_callback_sync(GatewayRunner *self, const char *session_key,
                             int iteration, const char *tool_names_json);

/* _event_callback_sync — emit generic event hook */
void gw_event_callback_sync(GatewayRunner *self, const char *session_key,
                              const char *event_type, const char *context_json);

/* _status_callback_sync — emit status update to session adapter */
void gw_status_callback_sync(GatewayRunner *self, const char *session_key,
                               const char *event_type, const char *message);

/* run_sync — main agent run loop (delegates to Python) */
int gw_run_sync(GatewayRunner *self, const char *session_key, const char *event_json);

/* _sessions_map — get or create the _sessions JSON map */
json_t *gw_sessions_map(GatewayRunner *self);

/* _session_state — get or create session state object */
json_t *gw_session_state(GatewayRunner *self, const char *session_key);

/* _peek_session_state — read-only session state lookup */
json_t *gw_peek_session_state(GatewayRunner *self, const char *session_key);

/* _is_session_running — check if session has a running agent */
bool gw_is_session_running(GatewayRunner *self, const char *session_key);

/* _running_agent_items — list [session_key, agent] pairs for all running agents */
json_t *gw_running_agent_items(GatewayRunner *self);

/* _load_restart_after_turn_timeout — env var, default 30s */
double gw_load_restart_after_turn_timeout(void);

/* _prepare_busy_steer_text — transcribe voice for busy steer */
const char *gw_prepare_busy_steer_text(GatewayRunner *self, const char *event_json);

/* _await_active_work_before_restart — wait for in-flight work before stop */
bool gw_await_active_work_before_restart(GatewayRunner *self, double timeout_secs);

/* _log_background_resume_result — done-callback for boot-resume tasks */
void gw_log_background_resume_result(const char *task_name, bool cancelled,
                                       const char *error);

/* _session_stall_timeout_seconds — env var, default 300s */
double gw_session_stall_timeout_seconds(GatewayRunner *self);

/* _iter_gateway_adapters — iterate all gateway adapters */
json_t *gw_iter_gateway_adapters(GatewayRunner *self);

/* _session_activity_for_stall — get activity summary for stall detection */
json_t *gw_session_activity_for_stall(GatewayRunner *self, const char *session_key);

/* _check_session_stalls — check all sessions for stall conditions */
int gw_check_session_stalls(GatewayRunner *self, double timeout_seconds);

/* _session_stall_watcher — periodic stall detection thread */
void *gw_session_stall_watcher(void *arg);

/* _make_default_profile_message_handler — message handler for default profile */
void *gw_make_default_profile_message_handler(GatewayRunner *self);

/* _primary_message_handler — return primary message handler */
void *gw_primary_message_handler(GatewayRunner *self);

/* _adapter_credential_claim — claim exclusive credential resource */
json_t *gw_adapter_credential_claim(GatewayRunner *self, const char *platform,
                                      const char *adapter_json);

/* _adapter_listener_claim — claim exclusive listener resource */
json_t *gw_adapter_listener_claim(GatewayRunner *self, const char *platform,
                                    const char *adapter_json);

/* _dispatch_busy_slash_command — dispatch slash command while agent running */
const char *gw_dispatch_busy_slash_command(GatewayRunner *self, const char *session_key,
                                             const char *command_name, const char *args);

/* _busy_start_command — /start handler (platform ping) */
const char *gw_busy_start_command(GatewayRunner *self, const char *session_key);

/* _busy_egress_command — gateway status text */
const char *gw_busy_egress_command(GatewayRunner *self, const char *session_key);

/* _busy_stop_command — /stop handler */
const char *gw_busy_stop_command(GatewayRunner *self, const char *session_key);

/* _busy_new_command — /new handler */
const char *gw_busy_new_command(GatewayRunner *self, const char *session_key);

/* _busy_queue_command — /queue handler */
const char *gw_busy_queue_command(GatewayRunner *self, const char *session_key,
                                    const char *prompt);

/* _busy_steer_command — /steer handler */
const char *gw_busy_steer_command(GatewayRunner *self, const char *session_key,
                                    const char *prompt);

/* _busy_goal_command — /goal handler */
const char *gw_busy_goal_command(GatewayRunner *self, const char *session_key,
                                   const char *args);

/* _prepare_clarify_reply_text — build clarify reply for ambiguous messages */
const char *gw_prepare_clarify_reply_text(GatewayRunner *self, const char *event_json);

/* _is_relay_discord_channel_lane — check if Discord source is a channel lane */
bool gw_is_relay_discord_channel_lane(GatewayRunner *self, const char *source_json);

/* _relay_auto_thread_info — get auto-thread info for Discord relay */
json_t *gw_relay_auto_thread_info(GatewayRunner *self, const char *source_json);

/* _build_stream_consumer_config — build stream consumer config JSON */
json_t *gw_build_stream_consumer_config(GatewayRunner *self, const char *source_json,
                                          const char *scfg_json, const char *adapter_json,
                                          const char *on_missing_cursor);

/* _shutdown_gateway_health_export — shutdown OTLP health export runtime */
void gw_shutdown_health_export(GatewayRunner *self);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_RUN_HELPERS_H */