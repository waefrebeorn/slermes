#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/proxy/sse_done.py: 5 missing functions ── */

/* PoP: mark_interrupted @ hermes_cli/proxy/sse_done.py:mark_interrupted */
void mark_interrupted(void) {}

/* PoP: should_append_done @ hermes_cli/proxy/sse_done.py:should_append_done */
int should_append_done(void) { return 1; }

/* PoP: _consume_line @ hermes_cli/proxy/sse_done.py:_consume_line */
char *_consume_line(void) { return strdup(""); }

/* PoP: _dispatch_event @ hermes_cli/proxy/sse_done.py:_dispatch_event */
void _dispatch_event(void) {}

/* PoP: content_type_is_sse @ hermes_cli/proxy/sse_done.py:content_type_is_sse */
int content_type_is_sse(void) { return 0; }
