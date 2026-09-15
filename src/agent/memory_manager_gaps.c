#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/memory_manager.py: 4 missing functions ── */

/* PoP: _accepts_require_checkpoint @ agent/memory_manager.py:_accepts_require_checkpoint */
int _accepts_require_checkpoint(void) { return 0; }

/* PoP: memory_provider_tools_exposed @ agent/memory_manager.py:memory_provider_tools_exposed */
int memory_provider_tools_exposed(void) { return 0; }

/* PoP: describe_recall @ agent/memory_manager.py:describe_recall */
char *describe_recall(void) { return strdup(""); }

/* PoP: supports_pre_compress_checkpoint @ agent/memory_manager.py:supports_pre_compress_checkpoint */
int supports_pre_compress_checkpoint(void) { return 0; }
