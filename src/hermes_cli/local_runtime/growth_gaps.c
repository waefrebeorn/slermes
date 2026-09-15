#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/growth.py: 6 missing functions ── */

/* PoP: window_overrides_path @ hermes_cli/local_runtime/growth.py:window_overrides_path */
char *window_overrides_path(void) { return strdup(""); }

/* PoP: load_window_overrides @ hermes_cli/local_runtime/growth.py:load_window_overrides */
char *load_window_overrides(void) { return strdup("{}"); }

/* PoP: save_window_override @ hermes_cli/local_runtime/growth.py:save_window_override */
void save_window_override(void) {}

/* PoP: clear_window_override @ hermes_cli/local_runtime/growth.py:clear_window_override */
void clear_window_override(void) {}

/* PoP: is_managed_endpoint @ hermes_cli/local_runtime/growth.py:is_managed_endpoint */
int is_managed_endpoint(void) { return 0; }

/* PoP: maybe_grow_window @ hermes_cli/local_runtime/growth.py:maybe_grow_window */
char *maybe_grow_window(void) { return strdup("{}"); }
