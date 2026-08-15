/* Slermes C11 port — gateway/shutdown_flush.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: spool_dropped_transcript_message @ gateway/shutdown_flush.py:spool_dropped_transcript_message */
static json_t *spool_dropped_transcript_message(void) {
    return json_object();
}

/* PoP: drain_transcript_spool @ gateway/shutdown_flush.py:drain_transcript_spool */
static json_t *drain_transcript_spool(const char * session_id, json_t * replay) {
    return json_object();
}

