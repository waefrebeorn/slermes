/*
 * session_ids.c — C home for upstream Hermes session_rules/session_ids.py.
 *
 * PoP: w12_sessionids_83c40f_is_known_session_id @ session_rules/session_ids.py:is_known_session_id
 *
 * Manual, allocation-free implementation of SESSION_ID_RECOGNIZERS.  Every
 * recognizer except the first is anchored with '$', which in Python also matches
 * immediately before one trailing newline; we strip a single trailing '\n' for
 * those.  The first recognizer (^\d{8}_\d{6}_) is a prefix match only.
 */
#include "noc_w12_sessionids_83c40f.h"

#include <stddef.h>
#include <string.h>

static int c_is_digit(char c) { return c >= '0' && c <= '9'; }
static int c_is_hex(char c)  { return c_is_digit(c) || (c >= 'a' && c <= 'f'); }
static int c_is_alnum(char c) {
    return c_is_digit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static int c_is_jobchar(char c) {
    return c_is_alnum(c) || c == '.' || c == '_' || c == '-';
}
static int c_is_u8_var(char c) { return c == '8' || c == '9' || c == 'a' || c == 'b'; }

static int digits(const char *s, size_t n) {
    for (size_t i = 0; i < n; ++i) if (!c_is_digit(s[i])) return 0;
    return 1;
}
static int hexes(const char *s, size_t n) {
    for (size_t i = 0; i < n; ++i) if (!c_is_hex(s[i])) return 0;
    return 1;
}
static int startswith(const char *s, size_t n, const char *prefix) {
    return strncmp(s, prefix, n) == 0;
}

int w12_sessionids_83c40f_is_known_session_id(const char *value) {
    if (!value) return 0;
    size_t len = strlen(value);

    /* 1: ^\d{8}_\d{6}_  (prefix match, no '$') */
    if (len >= 16 && digits(value, 8) && value[8] == '_' &&
        digits(value + 9, 6) && value[15] == '_')
        return 1;

    size_t eff = len;
    if (eff > 0 && value[eff - 1] == '\n') eff--;  /* Python '$' before trailing \n */

    /* 2: ^cron_[A-Za-z0-9][A-Za-z0-9._-]*_\d{8}_\d{6}$ */
    if (eff >= 22 && startswith(value, 5, "cron_")) {
        if (value[eff - 16] == '_' && digits(value + eff - 15, 8) &&
            value[eff - 7] == '_' && digits(value + eff - 6, 6)) {
            size_t mid = eff - 16 - 5;
            if (mid >= 1) {
                size_t ok = 1;
                for (size_t i = 5; i < 5 + mid; ++i)
                    if (!c_is_jobchar(value[i])) { ok = 0; break; }
                if (ok) return 1;
            }
        }
    }

    /* 3: ^bg_(?:\d{6}_)?[0-9a-f]{6}$ */
    if (startswith(value, 3, "bg_")) {
        if (eff - 3 == 6 && hexes(value + 3, 6)) return 1;
        if (eff - 3 == 13 && digits(value + 3, 6) && value[9] == '_' &&
            hexes(value + 10, 6)) return 1;
    }

    /* 4: ^room_[0-9a-f]{32}$ */
    if (eff == 37 && startswith(value, 5, "room_") && hexes(value + 5, 32)) return 1;

    /* 5: ^api_\d{9,}_[0-9a-f]{8}$ */
    if (eff >= 22 && startswith(value, 4, "api_")) {
        if (value[eff - 9] == '_' && hexes(value + eff - 8, 8)) {
            size_t nd = (eff - 9) - 4;
            if (nd >= 9 && digits(value + 4, nd)) return 1;
        }
    }

    /* 6: ^api-[0-9a-f]{16}$ */
    if (eff == 20 && startswith(value, 4, "api-") && hexes(value + 4, 16)) return 1;

    /* 7: ^run_[0-9a-f]{32}$ */
    if (eff == 36 && startswith(value, 4, "run_") && hexes(value + 4, 32)) return 1;

    /* 8: ^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$ */
    if (eff == 36 && hexes(value, 8) && value[8] == '-' &&
        hexes(value + 9, 4) && value[13] == '-' && value[14] == '4' &&
        hexes(value + 15, 3) && value[18] == '-' && c_is_u8_var(value[19]) &&
        hexes(value + 20, 3) && value[23] == '-' && hexes(value + 24, 12))
        return 1;

    return 0;
}
