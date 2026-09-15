#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/dingtalk_auth.py: 2 missing functions ── */

/* PoP: begin_registration @ hermes_cli/dingtalk_auth.py:begin_registration */
char *begin_registration(void) { return strdup(""); }

/* PoP: poll_registration @ hermes_cli/dingtalk_auth.py:poll_registration */
char *poll_registration(void) { return strdup(""); }
