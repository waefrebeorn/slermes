#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_finalizer.py: 4 missing functions ── */

/* PoP: _assistant_row_missing_visible_text @ agent/turn_finalizer.py:_assistant_row_missing_visible_text */
int _assistant_row_missing_visible_text(void) { return 0; }

/* PoP: _fill_assistant_tail_content @ agent/turn_finalizer.py:_fill_assistant_tail_content */
char *_fill_assistant_tail_content(void) { return strdup(""); }

/* PoP: _record_kanban_budget_exhausted @ agent/turn_finalizer.py:_record_kanban_budget_exhausted */
void _record_kanban_budget_exhausted(void) {}

/* PoP: _clone_background_review_messages @ agent/turn_finalizer.py:_clone_background_review_messages */
char *_clone_background_review_messages(void) { return strdup("[]"); }
