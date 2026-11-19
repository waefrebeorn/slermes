#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/read_extract.py: 5 missing functions ── */

/* PoP: anydoc_missing_error @ tools/read_extract.py:_anydoc_missing_error */
char *anydoc_missing_error(void) { return strdup(""); }

/* PoP: hosted_ocr_config @ tools/read_extract.py:_hosted_ocr_config */
char *hosted_ocr_config(void) { return strdup("{}"); }

int hosted_ocr_available(void) { return 0; }

int needs_ocr_warning(void) { return 0; }

/* PoP: sheet_rows @ tools/read_extract.py:_sheet_rows */
char *sheet_rows(void) { return strdup("[]"); }
