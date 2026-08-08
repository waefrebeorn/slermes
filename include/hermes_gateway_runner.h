/* hermes_gateway_runner.h — GatewayRunner opaque struct + API
 *
 * Full C11 port of gateway/run.py GatewayRunner class.
 * Every class method becomes a C function taking GatewayRunner *self.
 * Opaque struct — consumers only see the API.
 */
#ifndef HERMES_GATEWAY_RUNNER_H
#define HERMES_GATEWAY_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "hermes_json.h"

/* ─── Opaque handle ───────────────────────────────────────────────── */
typedef struct GatewayRunner GatewayRunner;

/* ─── Enums mirrored from Python ──────────────────────────────────── */

#define GW_BUSY_INTERRUPT "interrupt"
#define GW_BUSY_QUEUE     "queue"
#define GW_BUSY_STEER     "steer"
#define GW_BUSY_DEFAULT   GW_BUSY_INTERRUPT

#define GW_NOTIF_ALL    "all"
#define GW_NOTIF_RESULT "result"
#define GW_NOTIF_ERROR  "error"
#define GW_NOTIF_OFF    "off"

/* ─── Construction / Destruction ──────────────────────────────────── */

/* Create a GatewayRunner with default state. config_path is YAML path. */
GatewayRunner *gateway_runner_create(const char *config_path);

/* Free all resources. Calls stop if running. */
void gateway_runner_destroy(GatewayRunner *self);

/* ─── Lifecycle ────────────────────────────────────────────────────── */

/* Start the gateway loop. Returns 0 on success. */
int  gateway_runner_start(GatewayRunner *self);

/* Request clean stop with reason. */
void gateway_runner_request_stop(GatewayRunner *self, const char *reason);

/* Request restart. detached=1 for background restart. */
int  gateway_runner_request_restart(GatewayRunner *self, int detached, int via_service);

/* Block until shutdown completes. */
void gateway_runner_wait_for_shutdown(GatewayRunner *self);

/* ─── State accessors ─────────────────────────────────────────────── */

bool   gateway_runner_is_running(const GatewayRunner *self);
bool   gateway_runner_is_draining(const GatewayRunner *self);
bool   gateway_runner_should_exit_cleanly(const GatewayRunner *self);
bool   gateway_runner_should_exit_with_failure(const GatewayRunner *self);
const char *gateway_runner_exit_reason(const GatewayRunner *self);
int    gateway_runner_exit_code(const GatewayRunner *self);
const char *gateway_runner_busy_input_mode(const GatewayRunner *self);
const char *gateway_runner_busy_text_mode(const GatewayRunner *self);
double gateway_runner_restart_drain_timeout(const GatewayRunner *self);
const char *gateway_runner_service_tier(const GatewayRunner *self);
bool   gateway_runner_show_reasoning(const GatewayRunner *self);
const char *gateway_runner_background_notif_mode(const GatewayRunner *self);
const char *gateway_runner_ephemeral_system_prompt(const GatewayRunner *self);
int    gateway_runner_max_concurrent_sessions(const GatewayRunner *self);
int    gateway_runner_running_agent_count(const GatewayRunner *self);
bool   gateway_runner_has_setup_skill(const GatewayRunner *self);
double gateway_runner_scale_to_zero_idle_timeout(const GatewayRunner *self);
int    gateway_runner_goal_max_turns(const GatewayRunner *self);
bool   gateway_runner_should_echo_stt_transcripts(const GatewayRunner *self);
bool   gateway_runner_startup_should_abort(const GatewayRunner *self);

/* ─── Running-agent registry (port of GatewayRunner._running_agents) ─── */

/* Register a session turn as in-flight. Python sets
 * _running_agents[session_key] = agent before the turn and clears it in a
 * finally; the C port does the same around run_conversation() so
 * _drain_active_agents() can wait for real in-flight work. Also clears the
 * agent's interrupted flag so a /stop on a previous turn can't abort the
 * next one (Python constructs a fresh Agent per turn). */
void gateway_runner_note_turn_begin(GatewayRunner *self,
                                    const char *session_key, void *agent);
void gateway_runner_note_turn_end(GatewayRunner *self,
                                  const char *session_key);

/* Port of GatewayRunner._interrupt_running_agents — request_hard_interrupt
 * on every registered in-flight agent (sets agent->interrupted so the
 * conversation loop unwinds). Called after the drain window times out. */
void gateway_runner_interrupt_running_agents(GatewayRunner *self,
                                             const char *reason);

/* ─── Session model / reasoning override state ────────────────────── */

bool gateway_runner_is_intentional_model_switch(const GatewayRunner *self,
                                                const char *session_key,
                                                const char *agent_model);
/* Returns malloc'd {"had_override": bool, "override": dict|null} snapshot. */
json_t *gateway_runner_snapshot_session_model_override(const GatewayRunner *self,
                                                       const char *session_key);
void gateway_runner_restore_session_model_override(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *snapshot);
void gateway_runner_restore_pending_one_turn_model_override(GatewayRunner *self,
                                                            const char *session_key);
void gateway_runner_set_session_reasoning_override(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *reasoning_config);

/* ─── Run generation tokens ───────────────────────────────────────── */

int  gateway_runner_begin_session_run_generation(GatewayRunner *self,
                                                 const char *session_key);
int  gateway_runner_invalidate_session_run_generation(GatewayRunner *self,
                                                      const char *session_key,
                                                      const char *reason);
bool gateway_runner_is_session_run_current(const GatewayRunner *self,
                                           const char *session_key,
                                           int generation);

/* ─── Agent cache ─────────────────────────────────────────────────── */

void gateway_runner_evict_cached_agent(GatewayRunner *self,
                                       const char *session_key);
void gateway_runner_cache_agent(GatewayRunner *self, const char *session_key,
                                void *agent);

/* ─── Service tier / MoA / sidecar notes / native images ─────────────── */

void gateway_runner_set_session_service_tier_override(GatewayRunner *self,
                                                      const char *session_key,
                                                      const char *service_tier,
                                                      bool clear);
const char *gateway_runner_resolve_session_service_tier(const GatewayRunner *self,
                                                        const char *session_key);
/* Returns malloc'd reasoning-config object or NULL. */
json_t *gateway_runner_resolve_session_reasoning_config(const GatewayRunner *self,
                                                        const char *session_key,
                                                        const char *model);
void gateway_runner_restore_moa_one_shot(GatewayRunner *self,
                                         bool moa_disable_after_turn,
                                         const json_t *moa_restore_override,
                                         const char *quick_key);
void gateway_runner_set_pending_turn_sidecar_notes(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *notes);
/* Returns malloc'd array (possibly empty). */
json_t *gateway_runner_consume_pending_turn_sidecar_notes(GatewayRunner *self,
                                                          const char *session_key);
/* Returns malloc'd array (possibly empty). */
json_t *gateway_runner_consume_pending_native_image_paths(GatewayRunner *self,
                                                          const char *session_key);
void gateway_runner_stage_pending_native_image_paths(GatewayRunner *self,
                                                     const char *session_key,
                                                     const json_t *paths);

/* Borrowed accessor: the live _session_model_overrides dict (do not free). */
json_t *gateway_runner_session_model_overrides(const GatewayRunner *self);

/* run.py _apply_session_model_override — mutate *io_model + merge runtime
 * kwargs from the /model session override. runtime_kwargs is owned by caller. */
void gateway_runner_apply_session_model_override(GatewayRunner *self,
                                                 const char *session_key,
                                                 char **io_model,
                                                 json_t *runtime_kwargs);

/* ─── Config resolution (pure) ────────────────────────────────────── */

/* Resolve busy_input_mode from config string. Pure stateless. */
const char *gw_resolve_busy_input_mode(const char *cfg_value);

/* Resolve busy_text_mode from input mode + legacy text mode. */
const char *gw_resolve_busy_text_mode(const char *cfg_input_mode,
                                        const char *legacy_text_mode);

/* Resolve service_tier from raw config string. */
const char *gw_resolve_service_tier(const char *raw);

/* Resolve show_reasoning from config. */
bool gw_resolve_show_reasoning(const char *cfg_val, int is_bool, int bool_val,
                                const char *default_cfg);

/* Resolve background notification mode. */
const char *gw_resolve_background_notif_mode(const char *cfg_raw,
                                              int is_bool, int bool_val);

/* Resolve restart drain timeout from string. */
double gw_resolve_restart_drain_timeout(const char *raw, double default_val);

/* ─── Static helpers (pure) ───────────────────────────────────────── */

/* Check if event text is a goal continuation. */
bool gw_is_goal_continuation_event(const char *text);

/* Parse /reasoning args into (value, persist_global). */
void gw_parse_reasoning_command_args(const char *raw_args,
                                      char *value_out, size_t value_size,
                                      bool *persist_global);

/* Extract guild_id from event JSON. */
long gw_get_guild_id(const char *event_json_source);

/* Redact a Matrix session key to a stable fingerprint. */
void gw_redact_matrix_session_key(const char *session_key,
                                   char *out, size_t out_size);

/* Sanitize a Telegram topic title (128 char max, strip emoji/control). */
void gw_sanitize_telegram_topic_title(const char *title,
                                       char *out, size_t out_size);

/* Sanitize a Discord thread title (100 char max). */
void gw_sanitize_discord_thread_title(const char *title,
                                       char *out, size_t out_size);

/* Extract delivery identity from completion JSON. */
const char *gw_completion_delivery_identity(const char *delivery_json);

/* Build ephemeral change key from session_key and prefix. */
void gw_ephemeral_change_key(const char *session_key, const char *prefix,
                              char *out, size_t out_size);

/* ─── Session management ──────────────────────────────────────────── */

/* Build session key from source. out must be 256+ bytes. */
void gateway_runner_session_key_for_source(const GatewayRunner *self,
                                            const void *source,
                                            char *out, size_t out_size);

/* Check if session key has an active agent running. */
bool gateway_runner_session_is_active(const GatewayRunner *self,
                                       const char *session_key);

/* Get the number of active sessions. */
int  gateway_runner_active_session_count(const GatewayRunner *self);
/* Active cron job count (from scheduler). */
int  gateway_runner_active_cron_job_count(const GatewayRunner *self);
/* Active API-server run count (0 in C — adapter work folds into the runner). */
int  gateway_runner_active_api_run_count(const GatewayRunner *self);

/* ─── Message handling ────────────────────────────────────────────── */

/* Process an inbound message event. Returns response string or NULL. */
/* PoP: gateway_runner_handle_message @ gateway/run.py:_handle_message */
int gateway_runner_handle_message(GatewayRunner *self,
                                   const char *event_json,
                                   char *response_out, size_t response_size);

/* ─── Slash commands ──────────────────────────────────────────────── */

/* Check if a command name is an allowed slash command. */
/* PoP: gateway_runner_check_slash_access @ gateway/run.py:_check_slash_access */
bool gateway_runner_check_slash_access(const GatewayRunner *self,
                                        const char *command_name);

/* ─── Platform adapter management ──────────────────────────────────── */

/* Connect a platform adapter by name. Returns 0 on success. */
int gateway_runner_connect_adapter(GatewayRunner *self,
                                    const char *platform_name);

/* Disconnect a platform adapter by name. */
void gateway_runner_disconnect_adapter(GatewayRunner *self,
                                        const char *platform_name);

/* Number of connected platform adapters (borrowed view). */
int gateway_runner_adapter_count(const GatewayRunner *self);

/* Borrowed adapter handle at index (NULL when out of range). */
void *gateway_runner_adapter_at(const GatewayRunner *self, int index);

#endif /* HERMES_GATEWAY_RUNNER_H */
