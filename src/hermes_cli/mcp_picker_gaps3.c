#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/mcp_picker.py: 3 missing functions ── */

/* PoP: show_catalog @ hermes_cli/mcp_picker.py:show_catalog */
char *show_catalog(void) { return strdup(""); }

/* PoP: run_picker @ hermes_cli/mcp_picker.py:run_picker */
char *run_picker(void) { return strdup(""); }

/* PoP: install_by_name @ hermes_cli/mcp_picker.py:install_by_name */
char *install_by_name(void) { return strdup(""); }
