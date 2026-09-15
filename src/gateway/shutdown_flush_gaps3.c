#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/shutdown_flush.py: 3 missing functions ── */

/* PoP: flush_overflow_to_file @ gateway/shutdown_flush.py:flush_overflow_to_file */
char *flush_overflow_to_file(void) { return strdup(""); }

/* PoP: spool_dropped_transcript_message @ gateway/shutdown_flush.py:spool_dropped_transcript_message */
char *spool_dropped_transcript_message(void) { return strdup(""); }

/* PoP: drain_transcript_spool @ gateway/shutdown_flush.py:drain_transcript_spool */
char *drain_transcript_spool(void) { return strdup(""); }
