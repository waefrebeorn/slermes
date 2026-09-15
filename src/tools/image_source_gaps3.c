#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/image_source.py: 2 missing functions ── */

/* PoP: _ensure_container_env @ tools/image_source.py:_ensure_container_env */
void _ensure_container_env(void) { }

/* PoP: resolve_local_source_to_data_url @ tools/image_source.py:resolve_local_source_to_data_url */
char *resolve_local_source_to_data_url(void) { return strdup(""); }
