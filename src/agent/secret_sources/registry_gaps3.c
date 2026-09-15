#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/secret_sources/registry.py: 1 missing functions ── */

/* PoP: list_plugin_sources @ agent/secret_sources/registry.py:list_plugin_sources */
char *list_plugin_sources(void) { return strdup(""); }
