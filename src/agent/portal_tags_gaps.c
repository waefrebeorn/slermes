#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/portal_tags.py: 3 missing functions ── */

/* PoP: set_affinity_scope @ agent/portal_tags.py:set_affinity_scope */
void set_affinity_scope(const char *scope) { (void)scope; }

/* PoP: reset_affinity_scope @ agent/portal_tags.py:reset_affinity_scope */
void reset_affinity_scope(void) {}

/* PoP: get_affinity_scope @ agent/portal_tags.py:get_affinity_scope */
char *get_affinity_scope(void) { return strdup(""); }
