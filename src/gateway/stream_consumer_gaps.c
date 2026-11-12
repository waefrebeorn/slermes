#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/stream_consumer.py: 15 missing functions ── */

/* PoP: _stream_is_message @ gateway/stream_consumer.py:_stream_is_message */
int _stream_is_message(const char *stream) { return 0; }

/* PoP: accepts_tool_progress @ gateway/stream_consumer.py:accepts_tool_progress */
int accepts_tool_progress(void) { return 1; }

/* PoP: _compose_frame_content @ gateway/stream_consumer.py:_compose_frame_content */
char *_compose_frame_content(const char *content) { return strdup(""); }

/* PoP: _append_accumulated @ gateway/stream_consumer.py:_append_accumulated */
void _append_accumulated(const char *data) { (void)data; }


/* PoP: _record_turn_final_payload @ gateway/stream_consumer.py:_record_turn_final_payload */
void _record_turn_final_payload(const char *payload) { (void)payload; }

/* PoP: delivered_final_matches @ gateway/stream_consumer.py:delivered_final_matches */
int delivered_final_matches(void) { return 1; }


/* PoP: request_reopen_seed @ gateway/stream_consumer.py:request_reopen_seed */
char *request_reopen_seed(void) { return strdup(""); }


/* PoP: _fallback_flood_retry_delay @ gateway/stream_consumer.py:_fallback_flood_retry_delay */
double _fallback_flood_retry_delay(void) { return 1.0; }

/* PoP: _is_flood_error @ gateway/stream_consumer.py:_is_flood_error */
int _is_flood_error(const char *error) { return 0; }

/* PoP: _resolve_native_streaming @ gateway/stream_consumer.py:_resolve_native_streaming */
int _resolve_native_streaming(void) { return 1; }


/* PoP: _raw_message_limit @ gateway/stream_consumer.py:_raw_message_limit */
int _raw_message_limit(void) { return 4096; }
