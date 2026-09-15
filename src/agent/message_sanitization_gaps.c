#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/message_sanitization.py: 3 missing functions ── */

/* PoP: serialized_messages_bytes @ agent/message_sanitization.py:serialized_messages_bytes */
int serialized_messages_bytes(void) { return 0; }

/* PoP: _looks_like_image_content_rejection @ agent/message_sanitization.py:_looks_like_image_content_rejection */
int _looks_like_image_content_rejection(void) { return 0; }

/* PoP: stale_thinking_reaches_wire @ agent/message_sanitization.py:stale_thinking_reaches_wire */
int stale_thinking_reaches_wire(void) { return 0; }
