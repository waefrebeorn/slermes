#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/shutdown_watchdog.py: 2 missing functions ── */

/* PoP: get_loop_tick_socket_path @ gateway/shutdown_watchdog.py:get_loop_tick_socket_path */
char *get_loop_tick_socket_path(void) { return strdup(""); }

/* PoP: _tick_socket_handler @ gateway/shutdown_watchdog.py:_tick_socket_handler */
void _tick_socket_handler(void) { }
