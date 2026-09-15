#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/review_idle_queue.py: 10 missing functions ── */

/* PoP: defer_mode @ agent/review_idle_queue.py:defer_mode */
char *defer_mode(void) { return strdup(""); }

/* PoP: defer_max_age_s @ agent/review_idle_queue.py:defer_max_age_s */
int defer_max_age_s(void) { return 0; }

/* PoP: review_targets_managed_local @ agent/review_idle_queue.py:review_targets_managed_local */
char *review_targets_managed_local(void) { return strdup("[]"); }

/* PoP: note_turn_started @ agent/review_idle_queue.py:note_turn_started */
void note_turn_started(void) {}

/* PoP: note_turn_finished @ agent/review_idle_queue.py:note_turn_finished */
void note_turn_finished(void) {}

/* PoP: _ensure_thread @ agent/review_idle_queue.py:_ensure_thread */
void _ensure_thread(void) {}

/* PoP: _quiet_for @ agent/review_idle_queue.py:_quiet_for */
int _quiet_for(void) { return 0; }

/* PoP: _pop_dispatchable @ agent/review_idle_queue.py:_pop_dispatchable */
char *_pop_dispatchable(void) { return strdup("{}"); }

/* PoP: _still_enabled @ agent/review_idle_queue.py:_still_enabled */
int _still_enabled(void) { return 1; }

/* PoP: _managed_server_idle @ agent/review_idle_queue.py:_managed_server_idle */
int _managed_server_idle(void) { return 1; }
