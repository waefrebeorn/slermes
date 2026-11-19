#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/partial_compress.py: 3 missing functions ── */

char *parse_partial_compress_args(void) { return strdup(""); }

char *split_history_for_partial_compress(void) { return strdup(""); }

char *rejoin_compressed_head_and_tail(void) { return strdup(""); }
