/*
 * port_nous_account.c
 *
 * Pure helpers ported from hermes_cli/nous_account.py.
 *
 * The module's real work is JWT decoding, urllib calls to the Nous Portal and
 * dataclass construction. What is left once those are supplied as facts:
 *
 *   nous_normalize_tier    <- nous_account.py:_normalize_tier
 *   nous_is_anonymous_tier <- nous_account.py:_is_anonymous_tier
 *   nous_policy_notice     <- nous_account.py:nous_policy_notice
 *
 * The last one reads a live policy flag in Python; the C form takes it as a
 * parameter so the caller owns the auth lookup.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ===========================================================================
 *  _normalize_tier  (nous_account.py:_normalize_tier)
 *
 *      value.strip().lower() if isinstance(value, str) else ""
 *
 *  Tier claims arrive verbatim from a JWT claim or the account payload, neither
 *  of which strips or casefolds, so compare them normalized - otherwise casing
 *  or padding turns the anonymous tier into a registered one. A non-string is
 *  "" (Python's isinstance guard), and NULL is the C spelling of that.
 *  Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _normalize_tier @ hermes_cli/nous_account.py:_normalize_tier */
char *nous_normalize_tier(const char *value)
{
    /* A non-string is "" (Python's isinstance guard); NULL is the C spelling
     * of that, so it returns "" too rather than NULL. */
    if (!value) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    const unsigned char *s = (const unsigned char *)value;
    size_t len = strlen(value);
    size_t start = 0;
    while (start < len && isspace(s[start])) start++;
    size_t end = len;
    while (end > start && isspace(s[end - 1])) end--;

    char *out = (char *)malloc((end - start) + 1);
    if (!out) return NULL;
    size_t n = 0;
    for (size_t i = start; i < end; i++)
        out[n++] = (char)tolower(s[i]);
    out[n] = '\0';
    return out;
}

/* ===========================================================================
 *  _is_anonymous_tier  (nous_account.py:_is_anonymous_tier)
 *
 *      account_info is not None and account_info.is_anonymous_tier
 *
 *  The dataclass becomes two facts: whether an account snapshot exists, and the
 *  flag on it. Both must hold.
 * =========================================================================== */

/* PoP: _is_anonymous_tier @ hermes_cli/nous_account.py:_is_anonymous_tier */
int nous_is_anonymous_tier(int has_account_info, int is_anonymous_tier)
{
    return has_account_info && is_anonymous_tier;
}

/* ===========================================================================
 *  nous_policy_notice  (nous_account.py:nous_policy_notice)
 *
 *      if not removed or nous_policy_present() is not True:
 *          return ""
 *      return ("Your organization restricts which models are available — "
 *              "models outside its policy are not listed.")
 *
 *  `policy_present` is the C spelling of nous_policy_present()'s Optional[bool]:
 *  1 = True, 0 = False, -1 = unknown (None). Only a definite True counts, so an
 *  unreadable claim can never label a full list as filtered.
 *  Returns a NEW string the caller frees ("" included).
 * =========================================================================== */

/* PoP: nous_policy_notice @ hermes_cli/nous_account.py:nous_policy_notice */
char *nous_policy_notice(int removed, int policy_present)
{
    const char *msg =
        "Your organization restricts which models are available — "
        "models outside its policy are not listed.";
    if (!removed || policy_present != 1) msg = "";

    char *out = (char *)malloc(strlen(msg) + 1);
    if (!out) return NULL;
    strcpy(out, msg);
    return out;
}
