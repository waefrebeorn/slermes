#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/terminal_env_registry.py: 2 missing functions ── */

/* PoP: plugin_backend_names @ agent/terminal_env_registry.py:plugin_backend_names */
char *plugin_backend_names(void) { return strdup(""); }

/* PoP: plugin_strip_env_keys @ agent/terminal_env_registry.py:plugin_strip_env_keys */
char *plugin_strip_env_keys(void) { return strdup(""); }
