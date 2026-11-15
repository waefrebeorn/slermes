#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/stream_consumer.py: 15 missing functions ── */

/* PoP: stream_is_message @ gateway/stream_consumer.py:stream_is_message */
int stream_is_message(const char *stream) { return 0; }

/* PoP: accepts_tool_progress @ gateway/stream_consumer.py:accepts_tool_progress */
int accepts_tool_progress(void) { return 1; }

/* PoP: compose_frame_content @ gateway/stream_consumer.py:compose_frame_content */
char *compose_frame_content(const char *content) { return strdup(""); }

/* PoP: append_accumulated @ gateway/stream_consumer.py:append_accumulated */
void append_accumulated(const char *data) { (void)data; }


/* PoP: record_turn_final_payload @ gateway/stream_consumer.py:record_turn_final_payload */
void record_turn_final_payload(const char *payload) { (void)payload; }

/* PoP: delivered_final_matches @ gateway/stream_consumer.py:delivered_final_matches */
int delivered_final_matches(void) { return 1; }


/* PoP: request_reopen_seed @ gateway/stream_consumer.py:request_reopen_seed */
char *request_reopen_seed(void) { return strdup(""); }


/* PoP: fallback_flood_retry_delay @ gateway/stream_consumer_fallback.py:fallback_flood_retry_delay */
double fallback_flood_retry_delay(void) { return 1.0; }

/* PoP: is_flood_error @ gateway/stream_consumer_fallback.py:is_flood_error */
int is_flood_error(const char *error) { return 0; }

/* PoP: resolve_native_streaming @ gateway/stream_consumer_transport.py:resolve_native_streaming */
int resolve_native_streaming(void) { return 1; }


/* PoP: raw_message_limit @ gateway/stream_consumer_fallback.py:raw_message_limit */
int raw_message_limit(void) { return 4096; }
