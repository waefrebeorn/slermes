#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/media_repair.py: 4 missing functions ── */

/* PoP: _computer_use_capture_basename @ gateway/media_repair.py:_computer_use_capture_basename */
char *_computer_use_capture_basename(void) { return strdup(""); }

/* PoP: _iter_computer_use_capture_paths @ gateway/media_repair.py:_iter_computer_use_capture_paths */
char *_iter_computer_use_capture_paths(void) { return strdup("[]"); }

/* PoP: repair_explicit_computer_use_media_paths @ gateway/media_repair.py:repair_explicit_computer_use_media_paths */
char *repair_explicit_computer_use_media_paths(void) { return strdup("{}"); }

/* PoP: _repair_explicit_computer_use_media_paths_inner @ gateway/media_repair.py:_repair_explicit_computer_use_media_paths_inner */
char *_repair_explicit_computer_use_media_paths_inner(void) { return strdup("{}"); }
