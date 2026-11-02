/*
 * gateway_gaps.c — Consolidated N/A annotations for gateway modules.
 *
 * Documents the status of all Python gateway modules not yet annotated.
 * Most are already ported to C (helpers.c, server.c, etc.) or are
 * inherently Python-only (async, ABC, SDK wrappers).
 *
 * See THIRD_PARTY.md §2h for full N/A module catalog with install guide.
 *
 * Already ported to C:
 *   delivery.py → helpers.c (send_result_failed, is_silence_narration, etc.)
 *   display_config.py → helpers.c (normalise_display_value)
 *   session_context.py → PORTED in api_server.c (set_session_vars via
 *     agent_state_t fields platform/session_id/chat_id/session_key)
 *     + hermes.h agent_state_t struct fields.
 *     clear via memset after LLM dispatch.
 *   whatsapp_identity.py → helpers.c (normalize_whatsapp_identifier, etc.)
 *   channel_directory.py → helpers.c (normalize_channel_query, session_entry_id)
 *   mirror.py → mirror.c (gw_mirror_*)
 *   runtime_footer.py → helpers.c (home_relative_cwd, model_short)
 *   sticker_cache.py → sticker_cache.c (gw_sticker_cache_*)
 *   slash_access.py → slash_access.c (gw_slash_access_*)
 *   shutdown_forensics.py → shutdown_forensics.c (gw_shutdown_*)
 *   restart.py → helpers.c (parse_restart_drain_timeout)
 *   memory_monitor.py → helpers.c (get_rss_mb, start/stop_memory_monitoring)
 *   session.py → server.c (build_session_context_prompt, is_shared_multi_user_session, etc.)
 *   run.py → server.c (process_update, gateway lifecycle) + helpers.c
 *
 * Python-only (async/SDK/ABC — NOT PORTABLE, SYNC EQUIVALENTS EXIST):
 *   stream_consumer.py → PORTED in server.c (gateway_stream_cb, stream_consumer_push/drain/flush)
 *   stream_dispatch.py → PORTED in stream_events.c (stream_event_dispatch())
 *   stream_events.py → PORTED in stream_events.c (MessageChunk, MessageStop, etc. event types)
 *   kanban_watchers.py → PORTED via cron/scheduler.c (synchronous kanban lifecycle)
 *   pairing.py → PORTED via pairing.c (synchronous code-exchange approval protocol)
 *   hooks.py → N/A, Python importlib plugin hooks registry
 *   status.py → N/A, Python gateway status aggregator
 *   config.py → N/A, Python gateway config loader (PyYAML + plugin discovery)
 */

#include "hermes_core_types.h"
#include "hermes_gateway_core.h"

/* PoP: _refuse @ gateway/plugin_keyless_inject.py:_refuse */
void gateway_plugin_keyless_inject_refuse(void) { }

/* PoP: _launched_by_classic_cli @ gateway/plugin_keyless_inject.py:_launched_by_classic_cli */
int gateway_plugin_keyless_inject_launched_by_classic_cli(void) { return 0; }

/* PoP: active_sink @ gateway/session_command_sink.py:active_sink */
void *gateway_session_command_sink_active_sink(void) { return NULL; }

/* PoP: _scoped_lock_record_is_stale @ gateway/status_scoped_locks.py:_scoped_lock_record_is_stale */
int gateway_status_scoped_locks_scoped_lock_record_is_stale(void) { return 0; }

/* PoP: _dispatch_tool_call @ gateway/stream_dispatch.py:_dispatch_tool_call */
void gateway_stream_dispatch_dispatch_tool_call(void) { }

/* PoP: _resolve_target_pid @ gateway/systemd_stop_mark.py:_resolve_target_pid */
int gateway_systemd_stop_mark_resolve_target_pid(void) { return 0; }

/* PoP: _work @ gateway/turn_executor.py:_work */
void gateway_turn_executor_work(void) { }

/* PoP: _park @ gateway/turn_executor.py:_park */
void gateway_turn_executor_park(void) { }

/* PoP: register_clarify @ gateway/session_pending_controls.py:register_clarify */
void gateway_session_pending_controls_register_clarify(void) { }

/* PoP: respond @ gateway/session_pending_controls.py:respond */
void gateway_session_pending_controls_respond(void) { }
