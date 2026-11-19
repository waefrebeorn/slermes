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


/* PoP: extract_item_text @ agent/native_compaction.py:_extract_item_text */
char *extract_item_text(void) { return strdup(""); }

int has_retainable_image_content(void) { return 0; }

/* PoP: is_summary_item @ agent/native_compaction.py:is_summary_item */
int is_summary_item(void) { return 0; }
