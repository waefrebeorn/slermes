#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/detect.py: 2 missing functions ── */

/* PoP: probe_port @ hermes_cli/local_runtime/detect.py:probe_port */
char *probe_port(void) { return strdup(""); }

/* PoP: detect_server @ hermes_cli/local_runtime/detect.py:detect_server */
char *detect_server(void) { return strdup(""); }
