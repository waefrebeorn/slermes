#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/platforms/signal.py: 4 missing functions ── */

/* PoP: sig_secret @ plugins/platforms/signal/adapter.py:sig_secret */
char *sig_secret(void) { return strdup(""); }

/* PoP: utf16_offsets @ plugins/platforms/signal/adapter.py:utf16_offsets */
char *utf16_offsets(void) { return strdup("[]"); }

/* PoP: styles_for_chunk @ plugins/platforms/signal/adapter.py:styles_for_chunk */
char *styles_for_chunk(void) { return strdup("[]"); }

/* PoP: split_signal_formatted_message @ plugins/platforms/signal/adapter.py:split_signal_formatted_message */
char *split_signal_formatted_message(void) { return strdup("[]"); }
