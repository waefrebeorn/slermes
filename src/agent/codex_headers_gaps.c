#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/codex_headers.py: 3 missing functions ── */

/* PoP: is_official_codex_base_url @ agent/codex_headers.py:is_official_codex_base_url */
int is_official_codex_base_url(void) { return 0; }

/* PoP: codex_cloudflare_headers @ agent/codex_headers.py:codex_cloudflare_headers */
char *codex_cloudflare_headers(void) { return strdup("{}"); }

/* PoP: apply_required_codex_headers @ agent/codex_headers.py:apply_required_codex_headers */
void apply_required_codex_headers(void) {}
