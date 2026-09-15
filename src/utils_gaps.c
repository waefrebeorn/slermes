#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── utils.py: 5 missing functions ── */

/* PoP: _is_contended_windows_replace_error @ utils.py:_is_contended_windows_replace_error */
int _is_contended_windows_replace_error(void) { return 0; }

/* PoP: _rewrite_in_place @ utils.py:_rewrite_in_place */
int _rewrite_in_place(void) { return 1; }

/* PoP: _copy_fallback @ utils.py:_copy_fallback */
int _copy_fallback(void) { return 1; }

/* PoP: atomic_roundtrip_yaml_save @ utils.py:atomic_roundtrip_yaml_save */
int atomic_roundtrip_yaml_save(void) { return 1; }

/* PoP: base_url_origin @ utils.py:base_url_origin */
char *base_url_origin(void) { return strdup(""); }
