#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/clipboard.py: 1 missing functions ── */

/* PoP: has_clipboard_image @ hermes_cli/clipboard.py:has_clipboard_image */
char *has_clipboard_image(void) { return strdup(""); }
