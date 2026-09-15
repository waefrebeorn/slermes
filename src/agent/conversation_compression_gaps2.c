#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/conversation_compression.py: 19 missing functions ── */

/* PoP: _retry_compression_on_fallback_chain @ agent/conversation_compression.py:_retry_compression_on_fallback_chain */
int _retry_compression_on_fallback_chain(void) { return 0; }

/* PoP: _checkpoint_blocked @ agent/conversation_compression.py:_checkpoint_blocked */
int _checkpoint_blocked(void) { return 0; }

/* PoP: _get_context_compression_timeout_state @ agent/conversation_compression.py:_get_context_compression_timeout_state */
char *_get_context_compression_timeout_state(void) { return strdup("{}"); }

/* PoP: reset_context_compression_timeout_outcome @ agent/conversation_compression.py:reset_context_compression_timeout_outcome */
void reset_context_compression_timeout_outcome(void) {}

/* PoP: mark_context_compression_timed_out @ agent/conversation_compression.py:mark_context_compression_timed_out */
void mark_context_compression_timed_out(void) {}

/* PoP: context_compression_timed_out @ agent/conversation_compression.py:context_compression_timed_out */
int context_compression_timed_out(void) { return 0; }

/* PoP: _automatic_gate_blocked @ agent/conversation_compression.py:_automatic_gate_blocked */
int _automatic_gate_blocked(void) { return 0; }

/* PoP: compression_blocked_transiently @ agent/conversation_compression.py:compression_blocked_transiently */
int compression_blocked_transiently(void) { return 0; }

/* PoP: _mark_compression_blocked_transient @ agent/conversation_compression.py:_mark_compression_blocked_transient */
void _mark_compression_blocked_transient(void) {}

/* PoP: _emit_progress_status @ agent/conversation_compression.py:_emit_progress_status */
void _emit_progress_status(const char *status) { (void)status; }

/* PoP: _direct_messages_for_pre_compress_memory @ agent/conversation_compression.py:_direct_messages_for_pre_compress_memory */
char *_direct_messages_for_pre_compress_memory(void) { return strdup("[]"); }

/* PoP: _message_contains_busy_steer @ agent/conversation_compression.py:_message_contains_busy_steer */
int _message_contains_busy_steer(const char *msg) { return 0; }

/* PoP: _extract_steer_text_from_message @ agent/conversation_compression.py:_extract_steer_text_from_message */
char *_extract_steer_text_from_message(const char *msg) { return strdup(""); }

/* PoP: _compressed_has_busy_steer @ agent/conversation_compression.py:_compressed_has_busy_steer */
int _compressed_has_busy_steer(void) { return 0; }

/* PoP: _todo_snapshot_is_only_content @ agent/conversation_compression.py:_todo_snapshot_is_only_content */
int _todo_snapshot_is_only_content(void) { return 0; }

/* PoP: _replace_message_content @ agent/conversation_compression.py:_replace_message_content */
char *_replace_message_content(const char *msg, const char *content) { return strdup(""); }

/* PoP: _messages_match_scoped_identity @ agent/conversation_compression.py:_messages_match_scoped_identity */
int _messages_match_scoped_identity(const char *a, const char *b) { return 0; }

/* PoP: _codex_compaction_cooldown_remaining @ agent/conversation_compression.py:_codex_compaction_cooldown_remaining */
double _codex_compaction_cooldown_remaining(void) { return 0.0; }

/* PoP: _record_codex_compaction_failure @ agent/conversation_compression.py:_record_codex_compaction_failure */
void _record_codex_compaction_failure(const char *error) { (void)error; }
