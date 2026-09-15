#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/authz_mixin.py: 6 missing functions ── */

/* PoP: _platform_declares_allowed_users_env @ gateway/authz_mixin.py:_platform_declares_allowed_users_env */
int _platform_declares_allowed_users_env(void) { return 0; }

/* PoP: _bech32_polymod @ gateway/authz_mixin.py:_bech32_polymod */
int _bech32_polymod(void) { return 0; }

/* PoP: _bech32_hrp_expand @ gateway/authz_mixin.py:_bech32_hrp_expand */
char *_bech32_hrp_expand(void) { return strdup(""); }

/* PoP: _convertbits @ gateway/authz_mixin.py:_convertbits */
char *_convertbits(void) { return strdup(""); }

/* PoP: _npub_to_hex @ gateway/authz_mixin.py:_npub_to_hex */
char *_npub_to_hex(void) { return strdup(""); }

/* PoP: _normalize_nostr_allow_entries @ gateway/authz_mixin.py:_normalize_nostr_allow_entries */
char *_normalize_nostr_allow_entries(void) { return strdup("[]"); }
