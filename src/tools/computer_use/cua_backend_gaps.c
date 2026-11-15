#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/computer_use/cua_backend.py: 4 missing functions ── */

/* PoP: resolve_cua_driver_app_path @ tools/computer_use/cua_backend_daemon.py:resolve_cua_driver_app_path */
char *resolve_cua_driver_app_path(void) { return strdup(""); }

/* PoP: validate_cua_driver_app_signature @ tools/computer_use/cua_backend_daemon.py:validate_cua_driver_app_signature */
int validate_cua_driver_app_signature(void) { return 1; }

/* PoP: embedded_daemon_spawn_command @ tools/computer_use/cua_backend_daemon.py:embedded_daemon_spawn_command */
char *embedded_daemon_spawn_command(void) { return strdup("[]"); }

/* PoP: capture_full_screen @ tools/computer_use/cua_backend_capture.py:capture_full_screen */
char *capture_full_screen(void) { return strdup("{}"); }
