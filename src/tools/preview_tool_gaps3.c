#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/preview_tool.py: 3 missing functions ── */

/* PoP: preview_open @ tools/preview_tool.py:preview_open */
char *preview_open(void) { return strdup(""); }

/* PoP: preview_close @ tools/preview_tool.py:preview_close */
char *preview_close(void) { return strdup(""); }

/* PoP: _handle_preview @ tools/preview_tool.py:_handle_preview */
void _handle_preview(void) { }
