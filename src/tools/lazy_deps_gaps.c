#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/lazy_deps.py: 4 missing functions ── */

/* PoP: _installed_dist_roots @ tools/lazy_deps.py:_installed_dist_roots */
char *_installed_dist_roots(void) { return strdup("[]"); }

/* PoP: _warm_installed_bytecode @ tools/lazy_deps.py:_warm_installed_bytecode */
void _warm_installed_bytecode(void) {}

/* PoP: restore_features @ tools/lazy_deps.py:restore_features */
void restore_features(void) {}

/* PoP: _refresh_features @ tools/lazy_deps.py:_refresh_features */
void _refresh_features(void) {}
