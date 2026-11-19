#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/shutdown_flush.py: 3 missing functions ── */

char *flush_overflow_to_file(void) { return strdup(""); }

char *spool_dropped_transcript_message(void) { return strdup(""); }

char *drain_transcript_spool(void) { return strdup(""); }
