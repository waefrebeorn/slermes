#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/transports/types.py: 1 missing functions ── */

/* PoP: bedrock_content_blocks @ agent/transports/types.py:bedrock_content_blocks */
char *bedrock_content_blocks(void) { return strdup(""); }
