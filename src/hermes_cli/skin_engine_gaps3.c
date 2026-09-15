#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/skin_engine.py: 2 missing functions ── */

/* PoP: list_skins @ hermes_cli/skin_engine.py:list_skins */
char *list_skins(void) { return strdup(""); }

/* PoP: set_active_skin @ hermes_cli/skin_engine.py:set_active_skin */
char *set_active_skin(void) { return strdup(""); }
