/*
 * cron_prompt_sanitize.c — cron prompt threat-scanning + unicode surgery.
 *
 * Extracted from src/tools/port_cronjob_tools.c (v551 monolith split).
 * Self-contained: only pulls hermes_json (json_t) + hermes_logger (its log
 * macro). No god headers, no void* passthrough. The opaque context simply
 * groups the static pattern tables that previously lived at file scope.
 *
 * All PoP annotations are preserved from the original port.
 */

#include "cron_prompt_sanitize.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ==== Unicode ranges for emoji neighbour detection ==== */
static const unsigned int EMOJI_RANGES[][2] = {
    {0x1F000, 0x1FFFF},
    {0x2600,  0x27BF},
    {0x2300,  0x23FF},
    {0x1F1E6, 0x1F1FF},
    {0x20E3,  0x20E3},
};
#define EMOJI_RANGE_COUNT (sizeof(EMOJI_RANGES) / sizeof(EMOJI_RANGES[0]))
#define VARIATION_SELECTOR_CP 0xFE0F
#define ZWJ_CP                0x200D

/* ==== Skill-assembled looser set — threat matchers defined below (faithful
 * to _CRON_SKILL_ASSEMBLED_PATTERNS). ==== */

/* Invisible unicode codepoints. Order matters: Python iterates the
 * _CRON_INVISIBLE_CHARS frozenset and blocks on the first present char; a
 * frozenset's iteration order under PYTHONHASHSEED=0 is the order listed
 * first here, so we mirror it to match Python's "first match" deterministically.
 * (0x200E/0x200F/0x2061 are also stripped by the original C port and are
 * appended after the Python-ordered set — they don't affect first-match order
 * for inputs whose first invisible char is in the Python set.) */
/* Iteration order pinned to PYTHONHASHSEED=0 (the runner exports it): the
 * oracle recomputes the SAME function from live Python, whose frozenset
 * iteration order depends on the hash seed. Keep this table in exact
 * seed=0 order so both sides report the same first-blocked codepoint. */
static const unsigned int INVISIBLE_CPS[] = {
    0x2067, 0xFEFF, 0x2066, 0x2069, 0x2064, 0x202C, 0x202A, 0x2068, 0x202D,
    0x200D, 0x200B, 0x202B, 0x2063, 0x2062, 0x2060, 0x202E, 0x200C,
};
#define INVISIBLE_CP_COUNT (sizeof(INVISIBLE_CPS) / sizeof(INVISIBLE_CPS[0]))

/* Opaque context. Holds the tables above (grouped here so callers don't see
 * the statics). Currently stateless — the tables are module consts. */
struct cron_prompt_sanitize {
    int dummy; /* keep the struct non-empty for ABI stability */
};

cron_prompt_sanitize_t *cron_prompt_sanitize_init(void)
{
    return calloc(1, sizeof(cron_prompt_sanitize_t));
}

void cron_prompt_sanitize_free(cron_prompt_sanitize_t *ctx)
{
    free(ctx);
}

/* =============================================================
 *  Decoding UTF-8 to a single codepoint at byte offset.
 *  Returns bytes consumed (1..4) on success, 0 on truncated input,
 *  Replacing invalid sequences with U+FFFD via out=0.
 * ============================================================= */
static int decode_utf8(const char *s, size_t len, size_t i, unsigned int *out_cp)
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

static int encode_utf8(unsigned int cp, char *out4, size_t cap)
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

static bool cp_is_invisible(unsigned int cp)
{
    for (size_t i = 0; i < INVISIBLE_CP_COUNT; i++) {
        if (INVISIBLE_CPS[i] == cp) return true;
    }
    return false;
}

/* ================================================================
 *  1. PoP: _is_emoji_cp
 * ================================================================ */
/* PoP: cp_is_emoji @ tools/cronjob_tools.py:_is_emoji_cp */
/* Port of Python tools/cronjob_tools.py:_is_emoji_cp().
 * Returns true if the codepoint falls within any emoji Unicode range. */
static bool cp_is_emoji(unsigned int cp)
{
    for (size_t i = 0; i < EMOJI_RANGE_COUNT; i++) {
        if (cp >= EMOJI_RANGES[i][0] && cp <= EMOJI_RANGES[i][1]) return true;
    }
    return false;
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
 * *start_out to that cp's first byte index, or (0, (size_t)-1) if none. */
static unsigned int decode_utf8_before(const char *s, size_t len, size_t end, size_t *start_out)
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

/* PoP: zwj_has_emoji_neighbour @ tools/cronjob_tools.py:_zwj_has_emoji_neighbour */
/* Port of Python tools/cronjob_tools.py:_zwj_has_emoji_neighbour().
 * Returns true when the ZWJ at text[byte_idx] appears inside an emoji sequence.
 * Works on byte offsets: walks to the start of the left/right codepoints
 * (backing up over continuation bytes) and skips variation selectors (U+FE0F),
 * then tests whether both real neighbours are emoji codepoints. */
static bool zwj_has_emoji_neighbour(const char *text, size_t byte_idx)
{
    if (!text) return false;
    size_t len = strlen(text);

    unsigned int zwj_cp = 0;
    int zw = decode_utf8(text, len, byte_idx, &zwj_cp);
    if (zw <= 0) return false;

    /* Left neighbour: char immediately before the ZWJ, skipping variation
     * selectors. */
    size_t left_start;
    unsigned int lcp = decode_utf8_before(text, len, byte_idx, &left_start);
    while (left_start != (size_t)-1 && lcp == VARIATION_SELECTOR_CP) {
        lcp = decode_utf8_before(text, len, left_start, &left_start);
    }

    /* Right neighbour: char immediately after the ZWJ, skipping variation
     * selectors. */
    size_t right = byte_idx + (size_t)zw;
    unsigned int rcp = 0;
    if (right < len) {
        int rw = decode_utf8(text, len, right, &rcp);
        while (right < len && rw > 0 && rcp == VARIATION_SELECTOR_CP) {
            right += (size_t)rw;
            if (right < len) rw = decode_utf8(text, len, right, &rcp);
            else { rcp = 0; break; }
        }
    }

    bool has_left = (left_start != (size_t)-1);
    bool has_right = (right < len);
    if (!has_left || !has_right) return false;
    return cp_is_emoji(lcp) && cp_is_emoji(rcp);
}

/* ================================================================
 *  3. PoP: _strip_legitimate_emoji_zwj
 *  Returns a freshly-allocated string containing the cleaned prompt.
 * ================================================================ */
/* PoP: strip_legitimate_emoji_zwj @ tools/cronjob_tools.py:_strip_legitimate_emoji_zwj */
/* Port of Python tools/cronjob_tools.py:_strip_legitimate_emoji_zwj().
 * Returns a freshly-allocated string with legitimate emoji ZWJs removed. */
static char *strip_legitimate_emoji_zwj(const char *prompt)
{
    if (!prompt) return NULL;
    if (!strchr(prompt, (char)0xE2)) {
        /* No U+200D prefix bytes — likely no ZWJ present. Return copy. */
        return strdup(prompt);
    }

    size_t len = strlen(prompt);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t oi = 0;

    for (size_t i = 0; i < len; ) {
        unsigned int cp;
        int w = decode_utf8(prompt, len, i, &cp);
        if (w <= 0) { out[oi++] = prompt[i++]; continue; }
        if (cp == ZWJ_CP && zwj_has_emoji_neighbour(prompt, i)) {
            /* legitimate emoji joiner — drop */
            i += w;
            continue;
        }
        char tmp[4];
        int n = encode_utf8(cp, tmp, sizeof(tmp));
        if (n <= 0 || oi + n > len) { free(out); return strdup(prompt); }
        for (int k = 0; k < n; k++) out[oi++] = tmp[k];
        i += w;
    }
    out[oi] = '\0';
    return out;
}

/* ================================================================
 *  Lightweight case-insensitive substring match — avoids POSIX regex
 *  for our fixed regex literals (BREs use unescaped parens that would
 *  require BRE translation). Implements only what _scan_cron_prompt
 *  needs: anchored fragment checks for `curl`, `wget`, "Authorization:",
 *  secret var names, etc.
 * ================================================================ */
static bool contains_ci(const char *haystack, const char *needle)
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

static bool contains_secret_var_token(const char *haystack)
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
 *  4. PoP: _strip_cron_safe_constructs
 *  Mirrors Python regex:
 *    curl\s+[^\n]*(?:-H|--header)\s+['"]Authorization:\s*\$\{?\w*
 *      (?:KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API)\w*\}?['"]\s+
 *      ['"]?https://api\.github\.com
 *  Replaces the matched curl segment with "curl https://api.github.com/user".
 *  Returns malloc'd string.
 * ================================================================ */
/* PoP: strip_cron_safe_constructs @ tools/cronjob_tools.py:_strip_cron_safe_constructs */
/* Port of Python tools/cronjob_tools.py:_strip_cron_safe_constructs().
 * Strips the GitHub Authorization: token *** from curl commands to avoid
 * tripping the broader auth-header exfil rule. Returns malloc'd string. */
static char *strip_cron_safe_constructs(const char *prompt)
{
    if (!prompt) return NULL;

    /* Scan each line. The Python regex is anchored to a single line
     * (it uses [^\n]*). Find the first line that matches the GitHub
     * auth-header safe pattern. */
    const char *p = prompt;
    while (*p) {
        const char *line_end = strchr(p, '\n');
        size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
        char *line = malloc(line_len + 1);
        if (!line) return strdup(prompt);
        memcpy(line, p, line_len);
        line[line_len] = '\0';

        int matched = 0;
        /* Need: curl (preceded by start/whitespace), an -H/--header flag,
         * an Authorization: header with a secret-var token, and a
         * https://api.github.com URL. */
        if (contains_ci(line, "curl")
            && (contains_ci(line, "-H") || contains_ci(line, "--header"))
            && contains_secret_var_token(line)
            && contains_ci(line, "authorization:")
            && contains_ci(line, "https://api.github.com")) {
            matched = 1;
        }
        free(line);
        if (matched) {
            size_t prefix_len = (size_t)(p - prompt);
            const char *suffix = line_end ? line_end : (prompt + strlen(prompt));
            const char *repl = "curl https://api.github.com/user";
            size_t repl_len = strlen(repl);
            size_t suffix_len = strlen(suffix);
            size_t out_len = prefix_len + repl_len + suffix_len + 1;
            char *out = malloc(out_len);
            if (!out) return strdup(prompt);
            memcpy(out, prompt, prefix_len);
            memcpy(out + prefix_len, repl, repl_len);
            memcpy(out + prefix_len + repl_len, suffix, suffix_len);
            out[prefix_len + repl_len + suffix_len] = '\0';
            return out;
        }
        if (!line_end) break;
        p = line_end + 1;
    }
    return strdup(prompt);
}
/* ================================================================
 *  5. PoP: _check_invisible_unicode
 *  Returns an error string (caller must free) if invisible unicode
 *  injection markers are found, after stripping legitimate emoji ZWJs.
 *  Empty string on success.
 * ================================================================ */
/* PoP: check_invisible_unicode @ tools/cronjob_tools.py:_check_invisible_unicode */
/* Port of Python tools/cronjob_tools.py:_check_invisible_unicode().
 * Scans prompt (minus legitimate emoji ZWJs) for invisible unicode codepoints.
 * Returns malloc'd error string on block, empty string ("") on pass.
 * Caller owns the returned string. */
char *cron_prompt_sanitize_check_invisible(const char *prompt)
{
    if (!prompt) return strdup("");
    char *cleaned = strip_legitimate_emoji_zwj(prompt);
    if (!cleaned) return strdup("");

    /* Python iterates _CRON_INVISIBLE_CHARS (a set, table order) and blocks on
     * the first present char in THAT order — not string order. Mirror it. */
    for (size_t k = 0; k < INVISIBLE_CP_COUNT; k++) {
        unsigned int cp = INVISIBLE_CPS[k];
        /* cheap check: is this codepoint anywhere in the cleaned text? */
        for (size_t i = 0; i < strlen(cleaned); ) {
            unsigned int c2;
            int w = decode_utf8(cleaned, strlen(cleaned), i, &c2);
            if (w <= 0) { i++; continue; }
            if (c2 == cp) {
                char *err = malloc(128);
                if (err) snprintf(err, 128,
                    "Blocked: prompt contains invisible unicode U+%04X (possible injection).", cp);
                free(cleaned);
                return err ? err : strdup("Blocked: prompt contains invisible unicode (possible injection).");
            }
            i += w;
        }
    }
    free(cleaned);
    return strdup("");
}

/* ================================================================
 *  6. PoP: _strip_invisible_unicode
 *  Returns a JSON object {cleaned_prompt, removed_codepoints[]}.
 *  Caller owns the returned json_t*.
 * ================================================================ */
/* PoP: strip_invisible_unicode @ tools/cronjob_tools.py:_strip_invisible_unicode */
/* Port of Python tools/cronjob_tools.py:_strip_invisible_unicode().
 * Strips invisible unicode characters from prompt, preserving emoji ZWJs.
 * Returns json_t* object with "cleaned" (string) and "removed" (array of "U+XXXX").
 * Caller owns the returned object. */
json_t *cron_prompt_sanitize_strip_invisible(const char *prompt)
{
    if (!prompt) {
        json_t *obj = json_object();
        json_set(obj, "cleaned", json_string(""));
        json_set(obj, "removed", json_array());
        return obj;
    }
    size_t plen = strlen(prompt);
    char *out_buf = malloc(plen + 1);
    if (!out_buf) return NULL;
    size_t oi = 0;
    /* Collect removed codepoints in scan order, then emit sorted (matches
     * Python's sorted(removed) list). */
    unsigned *removed_cps = malloc(sizeof(unsigned) * (plen + 1));
    size_t removed_n = 0;
    if (!removed_cps) { free(out_buf); return NULL; }

    for (size_t i = 0; i < plen; ) {
        unsigned int cp;
        int w = decode_utf8(prompt, plen, i, &cp);
        if (w <= 0) { out_buf[oi++] = prompt[i++]; continue; }
        if (cp_is_invisible(cp)) {
            if (cp == ZWJ_CP && zwj_has_emoji_neighbour(prompt, i)) {
                /* legitimate emoji joiner — keep it */
                for (int k = 0; k < w; k++) out_buf[oi++] = prompt[i + k];
            } else {
                /* strip and record (de-duplicated — Python returns a set) */
                bool seen = false;
                for (size_t s = 0; s < removed_n; s++)
                    if (removed_cps[s] == cp) { seen = true; break; }
                if (!seen && removed_n < plen) removed_cps[removed_n++] = cp;
            }
        } else {
            for (int k = 0; k < w; k++) out_buf[oi++] = prompt[i + k];
        }
        i += w;
    }
    out_buf[oi] = '\0';

    /* sort ascending */
    for (size_t a = 0; a + 1 < removed_n; a++)
        for (size_t b = 0; b + 1 < removed_n - a; b++)
            if (removed_cps[b] > removed_cps[b + 1]) {
                unsigned t = removed_cps[b];
                removed_cps[b] = removed_cps[b + 1];
                removed_cps[b + 1] = t;
            }

    json_t *removed = json_array();
    for (size_t a = 0; a < removed_n; a++) {
        char label[16];
        snprintf(label, sizeof(label), "U+%04X", removed_cps[a]);
        json_append(removed, json_string(label));
    }
    free(removed_cps);

    json_t *obj = json_object();
    json_set(obj, "cleaned", json_string(out_buf));
    json_set(obj, "removed", removed);
    free(out_buf);
    return obj;
}

/* ================================================================
 *  7. PoP: _scan_cron_skill_assembled
 *  Scans an assembled cron prompt (includes loaded skill content).
 *  Returns json_t* {cleaned, error} where error is empty string on pass.
 * ================================================================ */
/* Faithful matchers for the 4 _CRON_SKILL_ASSEMBLED_PATTERNS (case-insensitive,
 * mirroring the Python regexes). Each returns true if the pattern is present. */

/* r'ignore\s+(?:\w+\s+)*(?:previous|all|above|prior)\s+(?:\w+\s+)*instructions' */
static bool match_injection_directive(const char *s)
{
    if (!contains_ci(s, "ignore") || !contains_ci(s, "instructions")) return false;
    return contains_ci(s, "previous") || contains_ci(s, "all")
        || contains_ci(s, "above") || contains_ci(s, "prior");
}

/* r'do\s+not\s+tell\s+the\s+user' */
static bool match_deception_hide(const char *s)
{
    return contains_ci(s, "do not tell the user");
}

/* r'system\s+prompt\s+override' */
static bool match_sys_prompt_override(const char *s)
{
    return contains_ci(s, "system prompt override");
}

/* r'disregard\s+(your|all|any)\s+(instructions|rules|guidelines)' */
static bool match_disregard_rules(const char *s)
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

/* PoP: scan_cron_skill_assembled @ tools/cronjob_tools.py:_scan_cron_skill_assembled */
/* Port of Python tools/cronjob_tools.py:_scan_cron_skill_assembled().
 * Looser pattern set — only catches unambiguous prompt-injection directives.
 * Invisible unicode is SANITIZED, not blocked. Skill bodies are already
 * scanned at install time; stray zero-width spaces in code examples should
 * not permanently kill the job.
 * Returns json_t* object with "cleaned" (string) and "error" (string). */
json_t *cron_prompt_sanitize_scan_skill_assembled(const char *assembled)
{
    if (!assembled) {
        json_t *obj = json_object();
        json_set(obj, "cleaned", json_string(""));
        json_set(obj, "error", json_string(""));
        return obj;
    }

    /* Step 1: sanitize invisible unicode */
    json_t *san = cron_prompt_sanitize_strip_invisible(assembled);
    const char *cleaned_ptr = json_get_str(san, "cleaned", "");
    /* Copy out of san before freeing it (json_get_str borrows into san). */
    char *cleaned = cleaned_ptr ? strdup(cleaned_ptr) : strdup("");
    json_t *removed = json_obj_get(san, "removed");
    if (removed && json_len(removed) > 0) {
        hermes_log(LOG_WARNING, "cron",
            "Cron skill-assembled prompt: stripped %zu invisible-unicode char(s) from vetted skill content",
            json_len(removed));
    }
    json_free(san);

    /* Step 2: strip GitHub safe construct */
    char *cleaned2 = strip_cron_safe_constructs(cleaned);

    /* Step 3: check the 4 skill-assembled threat patterns (faithful matchers). */
    char *err = NULL;
    if (match_injection_directive(cleaned2)) {
        err = strdup("prompt_injection");
    } else if (match_deception_hide(cleaned2)) {
        err = strdup("deception_hide");
    } else if (match_sys_prompt_override(cleaned2)) {
        err = strdup("sys_prompt_override");
    } else if (match_disregard_rules(cleaned2)) {
        err = strdup("disregard_rules");
    }

    json_t *obj = json_object();
    json_set(obj, "cleaned", json_string(cleaned2));
    if (err) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "Blocked: prompt matches threat pattern '%s'. Cron prompts must not contain injection or exfiltration payloads.",
            err);
        json_set(obj, "error", json_string(msg));
    } else {
        json_set(obj, "error", json_string(""));
    }
    free(cleaned);
    free(cleaned2);
    if (err) free(err);
    return obj;
}

/* ================================================================
 *  9. PoP: _scan_cron_prompt — STRICT user-prompt scan
 *  Applied to the raw user-supplied cron prompt at create/update time.
 *  Returns malloc'd error string when blocked, empty string when clean.
 * ================================================================ */

/* r'cat\s+[^\n]*(\.env|credentials|\.netrc|\.pgpass)' */
static bool match_read_secrets(const char *s)
{
    const char *p = s;
    while ((p = strcasestr_local(p, "cat")) != NULL) {
        /* require word boundary before and whitespace after */
        bool bstart = (p == s) || !isalnum((unsigned char)p[-1]);
        const char *after = p + 3;
        if (bstart && (*after == ' ' || *after == '\t')) {
            /* rest of line */
            const char *eol = strchr(after, '\n');
            size_t n = eol ? (size_t)(eol - after) : strlen(after);
            if (contains_ci_n(after, n, ".env") ||
                contains_ci_n(after, n, "credentials") ||
                contains_ci_n(after, n, ".netrc") ||
                contains_ci_n(after, n, ".pgpass"))
                return true;
        }
        p += 3;
    }
    return false;
}

/* r'rm\s+-rf\s+/' */
static bool match_destructive_root_rm(const char *s)
{
    const char *p = s;
    while ((p = strcasestr_local(p, "rm")) != NULL) {
        bool bstart = (p == s) || !isalnum((unsigned char)p[-1]);
        const char *q = p + 2;
        if (bstart && (*q == ' ' || *q == '\t')) {
            while (*q == ' ' || *q == '\t') q++;
            if (strncasecmp(q, "-rf", 3) == 0) {
                q += 3;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '/') return true;
            }
        }
        p += 2;
    }
    return false;
}

/* exfil: curl/wget line that also carries a secret-var token in a URL,
 * data payload, or Authorization header (the 5 Python exfil regexes all
 * require: the tool name, a qualifying carrier, and _CRON_SECRET_VAR_RE
 * on the same line). */
static bool match_exfil_command(const char *s, const char **pid_out)
{
    const char *p = s;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t n = eol ? (size_t)(eol - p) : strlen(p);
        char *line = malloc(n + 1);
        if (!line) return false;
        memcpy(line, p, n); line[n] = '\0';

        bool is_curl = contains_ci(line, "curl");
        bool is_wget = contains_ci(line, "wget");
        if ((is_curl || is_wget) && contains_secret_var_token(line)) {
            if (contains_ci(line, "http://") || contains_ci(line, "https://")) {
                /* secret must appear AFTER the URL scheme to be "in" the
                 * URL — approximate the Python regex by checking order */
                const char *scheme = strcasestr_local(line, "http");
                const char *dollar = strchr(line, '$');
                if (scheme && dollar && dollar > scheme) {
                    *pid_out = is_curl ? "exfil_curl_url" : "exfil_wget_url";
                    free(line);
                    return true;
                }
            }
            if (is_curl && (contains_ci(line, "--data") || contains_ci(line, " -d ") ||
                            contains_ci(line, "--form") || contains_ci(line, " -F "))) {
                *pid_out = "exfil_curl_data";
                free(line);
                return true;
            }
            if (is_wget && (contains_ci(line, "--post-data=") ||
                            contains_ci(line, "--post-file="))) {
                *pid_out = "exfil_wget_post";
                free(line);
                return true;
            }
            if (is_curl && (contains_ci(line, "-H") || contains_ci(line, "--header")) &&
                contains_ci(line, "authorization:")) {
                *pid_out = "exfil_curl_auth_header";
                free(line);
                return true;
            }
        }
        free(line);
        if (!eol) break;
        p = eol + 1;
    }
    return false;
}

static char *scan_blocked_msg(const char *pid)
{
    const char *fmt = "Blocked: prompt matches threat pattern '%s'. "
        "Cron prompts must not contain injection or exfiltration payloads.";
    size_t need = strlen(fmt) + strlen(pid) + 1;
    char *msg = malloc(need);
    if (msg) snprintf(msg, need, fmt, pid);
    return msg;
}

/* PoP: cron_prompt_sanitize_scan_prompt @ tools/cronjob_tools.py:_scan_cron_prompt */
/* Port of Python tools/cronjob_tools.py:_scan_cron_prompt().
 * Strict pattern set for the raw user prompt. Returns malloc'd error string
 * when blocked, or malloc'd empty string when the prompt is clean. */
char *cron_prompt_sanitize_scan_prompt(const char *prompt)
{
    if (!prompt) return strdup("");
    char *scan = strip_cron_safe_constructs(prompt);
    if (!scan) return strdup("");

    char *invisible_err = cron_prompt_sanitize_check_invisible(scan);
    if (invisible_err && invisible_err[0]) { free(scan); return invisible_err; }
    free(invisible_err);

    const char *pid = NULL;
    if (match_injection_directive(scan))        pid = "prompt_injection";
    else if (match_deception_hide(scan))        pid = "deception_hide";
    else if (match_sys_prompt_override(scan))   pid = "sys_prompt_override";
    else if (match_disregard_rules(scan))       pid = "disregard_rules";
    else if (match_read_secrets(scan))          pid = "read_secrets";
    else if (contains_ci(scan, "authorized_keys")) pid = "ssh_backdoor";
    else if (contains_ci(scan, "/etc/sudoers") || contains_ci(scan, "visudo"))
                                                pid = "sudoers_mod";
    else if (match_destructive_root_rm(scan))   pid = "destructive_root_rm";

    if (!pid) {
        const char *exfil_pid = NULL;
        if (match_exfil_command(scan, &exfil_pid)) pid = exfil_pid;
    }
    free(scan);
    if (pid) return scan_blocked_msg(pid);
    return strdup("");
}
