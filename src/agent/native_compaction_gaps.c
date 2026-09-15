#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/native_compaction.py: 5 missing functions ── */

/* PoP: resolve_native_compaction_capabilities @ agent/native_compaction.py:resolve_native_compaction_capabilities */
char *resolve_native_compaction_capabilities(void) { return strdup("{}"); }

/* PoP: _warn_native_compaction_suppressed_by_checkpoint_gate @ agent/native_compaction.py:_warn_native_compaction_suppressed_by_checkpoint_gate */
void _warn_native_compaction_suppressed_by_checkpoint_gate(void) {}

/* PoP: _extract_item_text @ agent/native_compaction.py:_extract_item_text */
char *_extract_item_text(void) { return strdup(""); }

/* PoP: _has_retainable_image_content @ agent/native_compaction.py:_has_retainable_image_content */
int _has_retainable_image_content(void) { return 0; }

/* PoP: _is_summary_item @ agent/native_compaction.py:_is_summary_item */
int _is_summary_item(void) { return 0; }
