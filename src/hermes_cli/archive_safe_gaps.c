#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/archive_safe.py: 5 missing functions ── */

/* PoP: normalize_archive_parts @ hermes_cli/archive_safe.py:normalize_archive_parts */
char *normalize_archive_parts(void) { return strdup("[]"); }

/* PoP: make_targz @ hermes_cli/archive_safe.py:make_targz */
char *make_targz(void) { return strdup(""); }

/* PoP: safe_extract_targz @ hermes_cli/archive_safe.py:safe_extract_targz */
int safe_extract_targz(void) { return 1; }

/* PoP: archive_root_dirs @ hermes_cli/archive_safe.py:archive_root_dirs */
char *archive_root_dirs(void) { return strdup("[]"); }

/* PoP: copy_regular_files @ hermes_cli/archive_safe.py:copy_regular_files */
void copy_regular_files(void) {}
