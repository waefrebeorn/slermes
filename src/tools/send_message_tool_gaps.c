#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/send_message_tool.py: 4 missing functions ── */

/* PoP: prepare_send_message_platforms @ tools/send_message_tool.py:prepare_send_message_platforms */
char *prepare_send_message_platforms(void) { return strdup("[]"); }

/* PoP: resolve_send_target @ tools/send_message_targets.py:resolve_send_target */
char *resolve_send_target(void) { return strdup("{}"); }

/* PoP: bounded_send_error @ tools/send_message_tool.py:bounded_send_error */
char *bounded_send_error(void) { return strdup(""); }

/* PoP: send_live_adapter_media @ tools/send_message_tool.py:send_live_adapter_media */
char *send_live_adapter_media(void) { return strdup("{}"); }
