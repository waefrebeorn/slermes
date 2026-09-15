#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/load_progress.py: 5 missing functions ── */

/* PoP: _composite_percent @ hermes_cli/local_runtime/load_progress.py:_composite_percent */
int _composite_percent(void) { return 0; }

/* PoP: _endpoint @ hermes_cli/local_runtime/load_progress.py:_endpoint */
char *_endpoint(void) { return strdup(""); }

/* PoP: _ensure_watcher @ hermes_cli/local_runtime/load_progress.py:_ensure_watcher */
void _ensure_watcher(void) {}

/* PoP: get_loading_progress @ hermes_cli/local_runtime/load_progress.py:get_loading_progress */
char *get_loading_progress(void) { return strdup("{}"); }

/* PoP: get_prefill_progress @ hermes_cli/local_runtime/load_progress.py:get_prefill_progress */
char *get_prefill_progress(void) { return strdup("{}"); }
