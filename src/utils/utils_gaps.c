#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── utils.py: 5 missing functions ── */

int is_contended_windows_replace_error(void) { return 0; }

/* PoP: rewrite_in_place @ utils/files.py:_rewrite_in_place */
int rewrite_in_place(void) { return 1; }

/* PoP: copy_fallback @ utils/files.py:_copy_fallback */
int copy_fallback(void) { return 1; }

/* PoP: atomic_roundtrip_yaml_save @ utils/files.py:atomic_roundtrip_yaml_save */
int atomic_roundtrip_yaml_save(void) { return 1; }

/* PoP: base_url_origin @ utils/files.py:base_url_origin */
char *base_url_origin(void) { return strdup(""); }
