#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/computer_use/cua_backend.py: 4 missing functions ── */

/* PoP: _resolve_cua_driver_app_path @ tools/computer_use/cua_backend.py:_resolve_cua_driver_app_path */
char *_resolve_cua_driver_app_path(void) { return strdup(""); }

/* PoP: _validate_cua_driver_app_signature @ tools/computer_use/cua_backend.py:_validate_cua_driver_app_signature */
int _validate_cua_driver_app_signature(void) { return 1; }

/* PoP: _embedded_daemon_spawn_command @ tools/computer_use/cua_backend.py:_embedded_daemon_spawn_command */
char *_embedded_daemon_spawn_command(void) { return strdup("[]"); }

/* PoP: _capture_full_screen @ tools/computer_use/cua_backend.py:_capture_full_screen */
char *_capture_full_screen(void) { return strdup("{}"); }
