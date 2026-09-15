#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/plugin_index.py: 3 missing functions ── */

/* PoP: get_index_url @ hermes_cli/plugin_index.py:get_index_url */
char *get_index_url(void) { return strdup(""); }

/* PoP: _load_seed_entries @ hermes_cli/plugin_index.py:_load_seed_entries */
void _load_seed_entries(void) { }

/* PoP: _fetch_remote @ hermes_cli/plugin_index.py:_fetch_remote */
void _fetch_remote(void) { }
