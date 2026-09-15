#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/read_extract.py: 5 missing functions ── */

/* PoP: _anydoc_missing_error @ tools/read_extract.py:_anydoc_missing_error */
char *_anydoc_missing_error(void) { return strdup(""); }

/* PoP: _hosted_ocr_config @ tools/read_extract.py:_hosted_ocr_config */
char *_hosted_ocr_config(void) { return strdup("{}"); }

/* PoP: hosted_ocr_available @ tools/read_extract.py:hosted_ocr_available */
int hosted_ocr_available(void) { return 0; }

/* PoP: _needs_ocr_warning @ tools/read_extract.py:_needs_ocr_warning */
int _needs_ocr_warning(void) { return 0; }

/* PoP: _sheet_rows @ tools/read_extract.py:_sheet_rows */
char *_sheet_rows(void) { return strdup("[]"); }
