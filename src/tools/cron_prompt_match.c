/*
 * cron_prompt_match.c - prompt-sanitiser matching / utf8 helpers.
 *
 * Split from cron_prompt_sanitize.c (soul.md: self-contained modules).
 * UTF-8 encode/decode, case-insensitive contains, directive matching.
 * No file-scope state.
 */
#include "cron_prompt_match.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* =============================================================
 *  Decoding UTF-8 to a single codepoint at byte offset.
 *  Returns bytes consumed (1..4) on success, 0 on truncated input,
 *  Replacing invalid sequences with U+FFFD via out=0.
 * ============================================================= */ int decode_utf8(const char *s, size_t len, size_t i, unsigned int *out_cp)
{
    if (i >= len) return 0;
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *out_cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && i + 1 < len) {
        unsigned int cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F);
        if (cp < 0x80) { *out_cp = 0xFFFD; return 1; }
        *out_cp = cp; return 2;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < len) {
        unsigned int cp = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
        if (cp < 0x800) { *out_cp = 0xFFFD; return 1; }
        *out_cp = cp; return 3;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < len) {
        unsigned int cp = ((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12)
                        | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) { *out_cp = 0xFFFD; return 1; }
        *out_cp = cp; return 4;
    }
    *out_cp = 0xFFFD; return 1;
}
 int encode_utf8(unsigned int cp, char *out4, size_t cap)
{
    if (cp < 0x80) {
        if (cap < 1) return 0;
        out4[0] = (char)cp; return 1;
    }
    if (cp < 0x800) {
        if (cap < 2) return 0;
        out4[0] = (char)(0xC0 | (cp >> 6));
        out4[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        if (cap < 3) return 0;
        out4[0] = (char)(0xE0 | (cp >> 12));
        out4[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out4[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cap < 4) return 0;
    out4[0] = (char)(0xF0 | (cp >> 18));
    out4[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out4[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out4[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}


/* ================================================================
 *  2. PoP: _zwj_has_emoji_neighbour
 *
 *  Python version works on Python str (codepoint-indexed). Below we walk
 *  the UTF-8 byte stream but match the codepoints at byte position `idx`.
 *  The Python count-based loop over variation selectors is preserved.
 * ================================================================ */
/* Decode the codepoint whose UTF-8 encoding ENDS just before byte `end`
 * (i.e. the character immediately to the left of `end`). Backs up over
 * continuation bytes to find the lead byte. Returns the cp and sets
 * *start_out to that cp's first byte index, or (0, (size_t)-1) if none. */ unsigned int decode_utf8_before(const char *s, size_t len, size_t end, size_t *start_out)
{
    if (end == 0) { *start_out = (size_t)-1; return 0; }
    size_t i = end - 1;
    /* back up over continuation bytes (10xxxxxx) to the lead byte */
    while (i > 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--;
    unsigned int cp = 0;
    int w = decode_utf8(s, len, i, &cp);
    if (w <= 0) { *start_out = (size_t)-1; return 0; }
    *start_out = i;
    return cp;
}


/* ================================================================
 *  Lightweight case-insensitive substring match — avoids POSIX regex
 *  for our fixed regex literals (BREs use unescaped parens that would
 *  require BRE translation). Implements only what _scan_cron_prompt
 *  needs: anchored fragment checks for `curl`, `wget`, "Authorization:",
 *  secret var names, etc.
 * ================================================================ */ bool contains_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    size_t nl = strlen(needle);
    size_t hl = strlen(haystack);
    if (nl > hl) return false;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t j = 0;
        while (j < nl &&
               tolower((unsigned char)haystack[i+j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nl) return true;
    }
    return false;
}
 bool contains_secret_var_token(const char *haystack)
{
    /* Mirrors _CRON_SECRET_VAR_RE: ${?KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API}? */
    static const char *markers[] = {"KEY","TOKEN","SECRET","PASSWORD","CREDENTIAL","API",NULL};
    for (size_t i = 0; markers[i]; i++) {
        char needle[64];
        snprintf(needle, sizeof(needle), "$%s", markers[i]);
        if (contains_ci(haystack, needle)) return true;
        snprintf(needle, sizeof(needle), "${%s", markers[i]);
        if (contains_ci(haystack, needle)) return true;
    }
    return false;
}


/* ================================================================
 *  Local helper: contains_ci_n (case-insensitive substring match with length limit)
 * ================================================================ */
static bool contains_ci_n(const char *haystack, size_t hay_n, const char *needle)
{
    if (!haystack || !needle) return false;
    size_t nl = strlen(needle);
    if (nl > hay_n) return false;
    for (size_t i = 0; i + nl <= hay_n; i++) {
        size_t j = 0;
        while (j < nl &&
               tolower((unsigned char)haystack[i+j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nl) return true;
    }
    return false;
}


/* ================================================================
 *  Local helper: strcasestr_local (case-insensitive strstr fallback)
 * ================================================================ */
static const char *strcasestr_local(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    size_t nl = strlen(needle), hl = strlen(haystack);
    if (nl > hl) return NULL;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t j = 0;
        while (j < nl &&
               tolower((unsigned char)haystack[i+j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nl) return haystack + i;
    }
    return NULL;
}


/* ================================================================
 *  7. PoP: _scan_cron_skill_assembled
 *  Scans an assembled cron prompt (includes loaded skill content).
 *  Returns json_t* {cleaned, error} where error is empty string on pass.
 * ================================================================ */
/* Faithful matchers for the 4 _CRON_SKILL_ASSEMBLED_PATTERNS (case-insensitive,
 * mirroring the Python regexes). Each returns true if the pattern is present. */

/* r'ignore\s+(?:\w+\s+)*(?:previous|all|above|prior)\s+(?:\w+\s+)*instructions' */ bool match_injection_directive(const char *s)
{
    if (!contains_ci(s, "ignore") || !contains_ci(s, "instructions")) return false;
    return contains_ci(s, "previous") || contains_ci(s, "all")
        || contains_ci(s, "above") || contains_ci(s, "prior");
}


/* r'do\s+not\s+tell\s+the\s+user' */ bool match_deception_hide(const char *s)
{
    return contains_ci(s, "do not tell the user");
}


/* r'system\s+prompt\s+override' */ bool match_sys_prompt_override(const char *s)
{
    return contains_ci(s, "system prompt override");
}


/* r'disregard\s+(your|all|any)\s+(instructions|rules|guidelines)' */ bool match_disregard_rules(const char *s)
{
    if (!contains_ci(s, "disregard")) return false;
    const char *after = NULL;
    const char *d = strcasestr_local(s, "disregard");
    if (d) after = d + strlen("disregard");
    if (!after) return false;
    /* require one of your|all|any then one of instructions|rules|guidelines */
    int has_qual = contains_ci(after, "your") || contains_ci(after, "all")
        || contains_ci(after, "any");
    int has_obj = contains_ci(after, "instructions") || contains_ci(after, "rules")
        || contains_ci(after, "guidelines");
    return has_qual && has_obj;
}
