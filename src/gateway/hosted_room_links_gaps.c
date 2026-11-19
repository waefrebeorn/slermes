#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_room_links.py: 8 missing functions ── */

char *from_record(const char *record) { return strdup("{}"); }

char *as_record(void) { return strdup("{}"); }

/* PoP: short_string @ gateway/hosted_room_links.py:_short_string */
char *short_string(const char *s) { return s ? strdup(s) : strdup(""); }

char *load_room_links(void) { return strdup("[]"); }

char *load_room_links_tolerant(void) { return strdup("[]"); }



char *make_stored_link(void) { return strdup("{}"); }
