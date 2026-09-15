#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/terminal_notify.py: 4 missing functions ── */

/* PoP: _write_tty @ hermes_cli/terminal_notify.py:_write_tty */
void _write_tty(void) {}

/* PoP: osc9 @ hermes_cli/terminal_notify.py:osc9 */
void osc9(void) {}

/* PoP: warp_supported @ hermes_cli/terminal_notify.py:warp_supported */
int warp_supported(void) { return 0; }

/* PoP: warp_osc777 @ hermes_cli/terminal_notify.py:warp_osc777 */
void warp_osc777(void) {}
