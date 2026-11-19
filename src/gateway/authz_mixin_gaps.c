#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/authz_mixin.py: 6 missing functions ── */

/* PoP: platform_declares_allowed_users_env @ gateway/authz_mixin.py:platform_declares_allowed_users_env */
int platform_declares_allowed_users_env(void) { return 0; }

int bech32_polymod(void) { return 0; }

/* PoP: bech32_hrp_expand @ gateway/authz_mixin.py:_bech32_hrp_expand */
char *bech32_hrp_expand(void) { return strdup(""); }

/* PoP: convertbits @ gateway/authz_mixin.py:_convertbits */
char *convertbits(void) { return strdup(""); }

/* PoP: npub_to_hex @ gateway/authz_mixin.py:_npub_to_hex */
char *npub_to_hex(void) { return strdup(""); }

/* PoP: normalize_nostr_allow_entries @ gateway/authz_mixin.py:_normalize_nostr_allow_entries */
char *normalize_nostr_allow_entries(void) { return strdup("[]"); }
