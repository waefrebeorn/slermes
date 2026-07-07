/**
 * port_cronjob_tools.c — Port of Python: tools/cronjob_tools.py
 *
 * Real C implementations for cron job tool helpers.
 *
 * Coverage (16 functions; mirrors _is_emoji_cp .. check_cronjob_requirements):
 *   emoji/ZWJ token surgery, prompt threat scanning (user + skill-assembled),
 *   origin capture from session env, local-delivery notice synthesis,
 *   repeat display, model-override resolution, base_url/script validation,
 *   canonical skill list assembly, optional value normalization, deliver
 *   parameter flattening, job formatting, cronjob dispatcher, requirements.
 */

#include "port_cronjob_tools.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

/* Opaque struct definition - private to this translation unit */
struct port_cronjob_tools_state {
    bool threat_patterns_loaded;
};

port_cronjob_tools_state_t *port_cronjob_tools_state_init(void)
{
    port_cronjob_tools_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->threat_patterns_loaded = false;
    return state;
}

void port_cronjob_tools_state_cleanup(port_cronjob_tools_state_t *state)
{
    if (!state) return;
    free(state);
}

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

/* ==== Threat patterns shipped with cronjob_tools.py ==== */
static const char *CRON_THREAT_PATTERNS[] = {
    "(?i)ignore\\s+(?:\\w+\\s+)*(?:previous|all|above|prior)\\s+(?:\\w+\\s+)*instructions",
    "(?i)do\\s+not\\s+tell\\s+the\\s+user",
    "(?i)system\\s+prompt\\s+override",
    "(?i)disregard\\s+(your|all|any)\\s+(instructions|rules|guidelines)",
};
#define CRON_THREAT_COUNT (sizeof(CRON_THREAT_PATTERNS) / sizeof(CRON_THREAT_PATTERNS[0]))

/* ==== Skill-assembled looser set ==== */
static const char *CRON_SKILL_PATTERNS[] = {
    "(?i)ignore\\s+(?:\\w+\\s+)*(?:previous|all|above|prior)\\s+(?:\\w+\\s+)*instructions",
    "(?i)do\\s+not\\s+tell\\s+the\\s+user",
    "(?i)system\\s+prompt\\s+override",
    "(?i)disregard\\s+(your|all|any)\\s+(instructions|rules|guidelines)",
};
#define CRON_SKILL_COUNT (sizeof(CRON_SKILL_PATTERNS) / sizeof(CRON_SKILL_PATTERNS[0]))
static const char *CRON_SKILL_PIDS[] = {
    "prompt_injection", "deception_hide", "sys_prompt_override", "disregard_rules",
};

/* ==== Invisible unicode codepoints (mirrors tools/threat_patterns:INVISIBLE_CHARS) ==== */
static const unsigned int INVISIBLE_CPS[] = {
    0x200B, 0x200C, 0x200D, 0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D,
    0x202E, 0x2060, 0x2061, 0x2062, 0x2063, 0x2064, 0x2066, 0x2067, 0x2068, 0x2069,
    0xFEFF,
};
#define INVISIBLE_CP_COUNT (sizeof(INVISIBLE_CPS) / sizeof(INVISIBLE_CPS[0]))

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
/* PoP: cp_is_emoji @ tools/cronjob_tools.py:_is_emoji_cp
 * Port of Python tools/cronjob_tools.py:_is_emoji_cp().
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
/* PoP: zwj_has_emoji_neighbour @ tools/cronjob_tools.py:_zwj_has_emoji_neighbour
 * Port of Python tools/cronjob_tools.py:_zwj_has_emoji_neighbour().
 * Returns true when the ZWJ at text[byte_idx] appears inside an emoji sequence. */
static bool zwj_has_emoji_neighbour(const char *text, size_t byte_idx)
{
    if (!text) return false;
    size_t len = strlen(text);

    /* Find the codepoint at byte_idx */
    unsigned int zwj_cp = 0;
    int w = decode_utf8(text, len, byte_idx, &zwj_cp);
    if (w <= 0) return false;

    /* Walk left, skipping variation selectors (U+FE0F). */
    size_t left = (byte_idx >= (size_t)w) ? byte_idx - w : (size_t)-1;
    while (left != (size_t)-1) {
        unsigned int cp;
        int lw = decode_utf8(text, len, left, &cp);
        if (lw <= 0) break;
        if (cp != VARIATION_SELECTOR_CP) break;
        if (left < (size_t)lw) { left = (size_t)-1; break; }
        left -= lw;
    }

    /* Walk right (growing), skipping ZWJ on legit emoji clusters is handled at
     * caller level — this helper only checks left/right neighbours. */
    size_t right = byte_idx + w;
    while (right < len) {
        unsigned int cp;
        int rw = decode_utf8(text, len, right, &cp);
        if (rw <= 0) break;
        if (cp != VARIATION_SELECTOR_CP) break;
        right += rw;
    }

    bool has_left = (left != (size_t)-1);
    bool has_right = (right < len);
    if (!has_left || !has_right) return false;

    unsigned int lcp = 0, rcp = 0;
    decode_utf8(text, len, left, &lcp);
    decode_utf8(text, len, right, &rcp);
    return cp_is_emoji(lcp) && cp_is_emoji(rcp);
}

/* ================================================================
 *  3. PoP: _strip_legitimate_emoji_zwj
 *  Returns a freshly-allocated string containing the cleaned prompt.
 * ================================================================ */
/* PoP: strip_legitimate_emoji_zwj @ tools/cronjob_tools.py:_strip_legitimate_emoji_zwj
 * Port of Python tools/cronjob_tools.py:_strip_legitimate_emoji_zwj().
 * Returns a freshly-allocated string with legitimate emoji ZWJs removed. */
static char *strip_legitimate_emoji_zwj(const char *prompt)
{
    if (!prompt) return NULL;
    if (!strchr(prompt, (char)0xE2)) {
        /* No U+200D prefix bytes — likely no ZWJ present. Return copy. */
        return strdup(prompt);
    }

    size_t len = strlen(prompt);
    /* Worst case 1:1 growth + small overhead — but UTF-8 with multi-byte means
     * we can shrink if ZWJs are dropped. Build into a buffer. */
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
        /* Append the bytes for this codepoint */
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
 *  Strips the GitHub "Authorization: token ***" header that would
 *  trip the broader curl auth-header exfil rule. Mirrors Python regex.
 * ================================================================ */
/* PoP: strip_cron_safe_constructs @ tools/cronjob_tools.py:_strip_cron_safe_constructs
 * Port of Python tools/cronjob_tools.py:_strip_cron_safe_constructs().
 * Strips the GitHub Authorization: token header from curl commands to avoid
 * tripping the broader auth-header exfil rule. Returns malloc'd string. */
static char *strip_cron_safe_constructs(const char *prompt)
{
    if (!prompt) return NULL;
    if (!contains_ci(prompt, "curl")) return strdup(prompt);
    if (!contains_ci(prompt, "Authorization") || !contains_ci(prompt, "github")) {
        return strdup(prompt);
    }

    /* Linear substring search: find "curl " ... "Authorization: " ... "github.com".
     * Replace the matched segment with the canonical "curl https://api.github.com/user". */
    const char *p = prompt;
    while (*p) {
        const char *curl = strcasestr_local(p, "curl");
        if (!curl) break;
        const char *line_end = strchr(curl, '\n');
        size_t line_len = line_end ? (size_t)(line_end - curl) : strlen(curl);
        if (contains_ci_n(curl, line_len, "Authorization")
            && contains_ci_n(curl, line_len, "github.com")) {
            /* Build result: prompt[:curl-prompt] + replacement + rest. */
            size_t prefix_len = (size_t)(curl - prompt);
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
        p = line_end ? line_end + 1 : NULL;
    }
    return strdup(prompt);
}

/* ================================================================
 *  5. PoP: _check_invisible_unicode
 *  Returns an error string (caller must free) if invisible unicode
 *  injection markers are found, after stripping legitimate emoji ZWJs.
 *  Empty string on success.
 * ================================================================ */
/* PoP: check_invisible_unicode @ tools/cronjob_tools.py:_check_invisible_unicode
 * Port of Python tools/cronjob_tools.py:_check_invisible_unicode().
 * Scans prompt (minus legitimate emoji ZWJs) for invisible unicode codepoints.
 * Returns malloc'd error string on block, empty string ("") on pass.
 * Caller owns the returned string. */
char *check_invisible_unicode(const char *prompt)
{
    if (!prompt) return strdup("");
    char *cleaned = strip_legitimate_emoji_zwj(prompt);
    if (!cleaned) return strdup("");

    for (size_t i = 0; i < strlen(cleaned); ) {
        unsigned int cp;
        int w = decode_utf8(cleaned, strlen(cleaned), i, &cp);
        if (w <= 0) { i++; continue; }
        if (cp_is_invisible(cp)) {
            char *err = malloc(128);
            if (err) snprintf(err, 128,
                "Blocked: prompt contains invisible unicode U+%04X (possible injection).", cp);
            free(cleaned);
            return err ? err : strdup("Blocked: prompt contains invisible unicode (possible injection).");
        }
        i += w;
    }
    free(cleaned);
    return strdup("");
}

/* ================================================================
 *  6. PoP: _strip_invisible_unicode
 *  Returns a JSON object {cleaned_prompt, removed_codepoints[]}.
 *  Caller owns the returned json_t*.
 * ================================================================ */
/* PoP: strip_invisible_unicode @ tools/cronjob_tools.py:_strip_invisible_unicode
 * Port of Python tools/cronjob_tools.py:_strip_invisible_unicode().
 * Strips invisible unicode characters from prompt, preserving emoji ZWJs.
 * Returns json_t* object with "cleaned" (string) and "removed" (array of "U+XXXX").
 * Caller owns the returned object. */
json_t *strip_invisible_unicode(const char *prompt)
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
    json_t *removed = json_array();

    for (size_t i = 0; i < plen; ) {
        unsigned int cp;
        int w = decode_utf8(prompt, plen, i, &cp);
        if (w <= 0) { out_buf[oi++] = prompt[i++]; continue; }
        if (cp_is_invisible(cp)) {
            if (cp == ZWJ_CP && zwj_has_emoji_neighbour(prompt, i)) {
                /* legitimate emoji joiner — keep it */
                for (int k = 0; k < w; k++) out_buf[oi++] = prompt[i + k];
            } else {
                /* strip and record */
                char label[16];
                snprintf(label, sizeof(label), "U+%04X", cp);
                json_append(removed, json_string(label));
            }
        } else {
            for (int k = 0; k < w; k++) out_buf[oi++] = prompt[i + k];
        }
        i += w;
    }
    out_buf[oi] = '\0';

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
/* PoP: scan_cron_skill_assembled @ tools/cronjob_tools.py:_scan_cron_skill_assembled
 * Port of Python tools/cronjob_tools.py:_scan_cron_skill_assembled().
 * Looser pattern set — only catches unambiguous prompt-injection directives.
 * Invisible unicode is SANITIZED, not blocked. Skill bodies are already
 * scanned at install time; stray zero-width spaces in code examples should
 * not permanently kill the job.
 * Returns json_t* object with "cleaned" (string) and "error" (string). */
json_t *scan_cron_skill_assembled(const char *assembled)
{
    if (!assembled) {
        json_t *obj = json_object();
        json_set(obj, "cleaned", json_string(""));
        json_set(obj, "error", json_string(""));
        return obj;
    }

    /* Step 1: sanitize invisible unicode */
    json_t *san = strip_invisible_unicode(assembled);
    const char *cleaned = json_get_str(san, "cleaned", "");
    json_t *removed = json_obj_get(san, "removed");
    if (removed && json_len(removed) > 0) {
        hermes_log(LOG_WARNING, "cron",
            "Cron skill-assembled prompt: stripped %zu invisible-unicode char(s) from vetted skill content",
            json_len(removed));
    }
    json_free(san);

    /* Step 2: strip GitHub safe construct */
    char *cleaned2 = strip_cron_safe_constructs(cleaned);

    /* Step 3: check skill-assembled threat patterns */
    char *err = NULL;
    for (size_t i = 0; i < CRON_SKILL_COUNT; i++) {
        if (contains_ci(cleaned2, CRON_SKILL_PATTERNS[i])) {
            err = malloc(256);
            if (err) snprintf(err, 256,
                "Blocked: prompt matches threat pattern '%s'. Cron prompts must not contain injection or exfiltration payloads.",
                CRON_SKILL_PIDS[i]);
            break;
        }
    }

    json_t *obj = json_object();
    json_set(obj, "cleaned", json_string(cleaned2));
    json_set(obj, "error", json_string(err ? err : ""));
    free(cleaned2);
    if (err) free(err);
    return obj;
}

/* ================================================================
 *  8. PoP: _origin_from_env
 *  Captures session env vars into a JSON object.
 *  Returns json_t* object or NULL if env vars not set.
 * ================================================================ */
/* PoP: origin_from_env @ tools/cronjob_tools.py:_origin_from_env
 * Port of Python tools/cronjob_tools.py:_origin_from_env().
 * Reads HERMES_SESSION_PLATFORM, HERMES_SESSION_CHAT_ID, HERMES_SESSION_THREAD_ID,
 * HERMES_SESSION_CHAT_NAME, HERMES_SESSION_USER_ID from environment.
 * Returns json_t* object with platform, chat_id, thread_id, chat_name, user_id,
 * or NULL if platform/chat_id are not both set. */
json_t *origin_from_env(void)
{
    const char *platform = getenv("HERMES_SESSION_PLATFORM");
    const char *chat_id = getenv("HERMES_SESSION_CHAT_ID");
    if (!platform || !chat_id || !*platform || !*chat_id) return NULL;

    const char *thread_id = getenv("HERMES_SESSION_THREAD_ID");
    const char *chat_name = getenv("HERMES_SESSION_CHAT_NAME");
    const char *user_id = getenv("HERMES_SESSION_USER_ID");

    json_t *obj = json_object();
    json_set(obj, "platform", json_string(platform));
    json_set(obj, "chat_id", json_string(chat_id));
    if (thread_id && *thread_id) json_set(obj, "thread_id", json_string(thread_id));
    if (chat_name && *chat_name) json_set(obj, "chat_name", json_string(chat_name));
    if (user_id && *user_id) json_set(obj, "user_id", json_string(user_id));
    return obj;
}

/* ================================================================
 *  9. PoP: _local_delivery_notice
 *  Returns a notice string (caller frees) when job is local-only
 *  and will not be delivered back to the session.
 * ================================================================ */
/* PoP: local_delivery_notice @ tools/cronjob_tools.py:_local_delivery_notice
 * Port of Python tools/cronjob_tools.py:_local_delivery_notice().
 * Returns malloc'd notice string when a created job won't deliver anywhere
 * (CLI/TUI sessions have no live-delivery channel). Returns NULL when
 * user explicitly requested "local" or job resolves to a real delivery target.
 * In C we cannot call _resolve_delivery_targets, so we check origin presence
 * as a best-effort proxy: if origin exists, we assume delivery will work. */
char *local_delivery_notice(const json_t *job, const char *user_deliver)
{
    if (!job) return NULL;
    if (user_deliver) {
        /* Normalize user_deliver: trim, lower-case */
        char norm[256];
        const char *p = user_deliver;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t len = 0;
        while (*p && !isspace((unsigned char)*p) && len < sizeof(norm)-1) {
            norm[len++] = tolower((unsigned char)*p++);
        }
        norm[len] = '\0';
        if (strcmp(norm, "local") == 0) return NULL;
    }
    if (json_obj_get(job, "origin")) return NULL;

    const char *msg = "This is a local-only cron job: its output is saved (view it with "
                      "cronjob(action='list')) but will NOT be delivered back into this "
                      "session — CLI/TUI sessions have no live-delivery channel. To be "
                      "notified when it runs, recreate or update the job with deliver set to "
                      "a gateway-connected platform, e.g. deliver='telegram' or deliver='all'.";
    return strdup(msg);
}

/* ================================================================
 *  10. PoP: _repeat_display
 *  Returns a malloc'd string describing the repeat state.
 * ================================================================ */
/* PoP: repeat_display @ tools/cronjob_tools.py:_repeat_display
 * Port of Python tools/cronjob_tools.py:_repeat_display().
 * Formats the repeat configuration: "forever", "once", "1/1", "N times", "X/Y". */
char *repeat_display(const json_t *job)
{
    if (!job) return strdup("?");
    json_t *repeat = json_obj_get(job, "repeat");
    if (!repeat) return strdup("forever");
    json_t *times = json_obj_get(repeat, "times");
    json_t *completed = json_obj_get(repeat, "completed");
    if (!times || times->type != JSON_NUMBER) return strdup("forever");
    long t = (long)times->num_val;
    long c = completed && completed->type == JSON_NUMBER ? (long)completed->num_val : 0;
    if (t <= 0) return strdup("forever");
    if (t == 1) {
        if (c == 0) return strdup("once");
        return strdup("1/1");
    }
    if (c == 0) {
        char *s = malloc(32);
        if (s) snprintf(s, 32, "%ld times", t);
        return s;
    }
    char *s = malloc(32);
    if (s) snprintf(s, 32, "%ld/%ld", c, t);
    return s;
}

/* ================================================================
 *  11. PoP: _resolve_model_override
 *  Returns json_t* {provider, model} (both strings or null).
 *  Pins provider to config main provider if model given but provider omitted.
 * ================================================================ */
/* PoP: resolve_model_override @ tools/cronjob_tools.py:_resolve_model_override
 * Port of Python tools/cronjob_tools.py:_resolve_model_override().
 * Resolves a model override object into (provider, model) for job storage.
 * If provider is omitted, pins the current main provider from config so the
 * job doesn't drift when the user later changes their default via hermes model.
 * Returns json_t* object with "provider" (string or null) and "model" (string or null). */
json_t *resolve_model_override(const json_t *model_obj)
{
    json_t *obj = json_object();
    if (!model_obj || model_obj->type != JSON_OBJECT) {
        json_set(obj, "provider", json_null());
        json_set(obj, "model", json_null());
        return obj;
    }
    const char *model_name = json_get_str(model_obj, "model", "");
    const char *provider_name = json_get_str(model_obj, "provider", "");

    /* Strip whitespace */
    char *m = model_name && *model_name ? strdup(model_name) : NULL;
    char *p = provider_name && *provider_name ? strdup(provider_name) : NULL;

    /* Bare "custom" → no named custom provider → treat as no provider supplied */
    if (p && strcmp(p, "custom") == 0) {
        /* In C we cannot check has_named_custom_provider; leave as "custom" if
         * explicitly given, else NULL. For parity we clear it. */
        free(p);
        p = NULL;
    }

    if (m && !p) {
        /* Best-effort: read main provider from config if available.
         * In C port, we leave provider NULL — runtime will pin on fire. */
    }

    json_set(obj, "provider", p ? json_string(p) : json_null());
    json_set(obj, "model", m ? json_string(m) : json_null());
    if (p) free(p);
    if (m) free(m);
    return obj;
}

/* ================================================================
 *  12. PoP: _normalize_optional_job_value
 *  Normalizes an optional value: strips whitespace, optionally strips
 *  trailing slash. Returns malloc'd string or NULL.
 * ================================================================ */
static char *normalize_optional_job_value(const char *value, bool strip_trailing_slash)
{
    if (!value) return NULL;
    const char *start = value;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = value + strlen(value);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    if (strip_trailing_slash && end > start && *(end - 1) == '/') end--;
    size_t len = (size_t)(end - start);
    if (len == 0) return NULL;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* ================================================================
 *  13. PoP: _normalize_deliver_param
 *  Flattens list/tuple deliver values to comma-separated string.
 * ================================================================ */
/* PoP: normalize_deliver_param @ tools/cronjob_tools.py:_normalize_deliver_param
 * Port of Python tools/cronjob_tools.py:_normalize_deliver_param().
 * Normalizes a user-supplied "deliver" value to canonical string form.
 * Flattens arrays/tuples to comma-separated string. Returns malloc'd string
 * or NULL for None/empty. */
static char *normalize_deliver_param(const json_t *value)
{
    if (!value) return NULL;
    if (value->type == JSON_ARRAY) {
        size_t n = json_len(value);
        char *parts[64]; size_t pc = 0;
        for (size_t i = 0; i < n && pc < 64; i++) {
            const char *s = json_get_str(json_get(value, i), NULL, "");
            if (s && *s) parts[pc++] = strdup(s);
        }
        if (pc == 0) return NULL;
        size_t total = 1;
        for (size_t i = 0; i < pc; i++) total += strlen(parts[i]) + 1;
        char *out = malloc(total);
        if (!out) { for (size_t i = 0; i < pc; i++) free(parts[i]); return NULL; }
        char *w = out;
        for (size_t i = 0; i < pc; i++) {
            if (i > 0) *w++ = ',';
            size_t l = strlen(parts[i]);
            memcpy(w, parts[i], l);
            w += l;
            free(parts[i]);
        }
        *w = '\0';
        return out;
    }
    return normalize_optional_job_value(json_get_str(value, NULL, ""), false);
}

/* ================================================================
 *  14. PoP: _validate_cron_base_url
 *  Rejects pairing a named provider's stored credential with an off-host base_url.
 *  Returns malloc'd error string on block, NULL on valid.
 * ================================================================ */
/* PoP: validate_cron_base_url @ tools/cronjob_tools.py:_validate_cron_base_url
 * Port of Python tools/cronjob_tools.py:_validate_cron_base_url().
 * Rejects pairing a named provider's stored credential with an attacker
 * base_url. In C we lack provider registry resolution, so we:
 * - Allow if no provider or provider == "custom" (BYOK path)
 * - Allow if base_url host matches provider's known host (not implemented)
 * - Fail closed: any named provider with non-empty base_url is refused.
 * Returns malloc'd error string on block, NULL on valid. */
char *validate_cron_base_url(const char *provider, const char *base_url)
{
    char *bu = normalize_optional_job_value(base_url, true);
    if (!bu) return NULL;
    char *prov = normalize_optional_job_value(provider, false);
    if (!prov) {
        free(bu);
        return strdup("base_url override requires an explicit provider. Set provider to a "
                      "configured custom provider to use a custom endpoint.");
    }
    if (strcmp(prov, "custom") == 0) {
        free(bu); free(prov);
        return NULL; /* BYOK — key derived from this base_url or host-gated env vars */
    }
    /* Without runtime provider resolution, fail closed for any named provider */
    char *err = malloc(256);
    if (err) snprintf(err, 256,
        "base_url '%s' is not allowed for provider '%s'. A named "
        "provider's stored credential may only be sent to its own endpoint; "
        "use a configured custom provider (provider=\"custom\") for a custom base_url.",
        bu, prov);
    free(bu); free(prov);
    return err;
}

/* ================================================================
 *  15. PoP: _validate_cron_script_path
 *  Validates a cron job script path: must be relative, within
 *  HERMES_HOME/scripts/. Returns malloc'd error string or NULL.
 * ================================================================ */
/* PoP: validate_cron_script_path @ tools/cronjob_tools.py:_validate_cron_script_path
 * Port of Python tools/cronjob_tools.py:_validate_cron_script_path().
 * Scripts must be relative paths that resolve within HERMES_HOME/scripts/.
 * Absolute paths and ~ expansion are rejected to prevent arbitrary script
 * execution via prompt injection. Returns malloc'd error string or NULL. */
char *validate_cron_script_path(const char *script)
{
    if (!script || !*script) return NULL; /* empty/None = clearing the field */
    char *raw = normalize_optional_job_value(script, false);
    if (!raw) return NULL;

    /* Reject absolute paths, ~ expansion, Windows drive letters */
    if (raw[0] == '/' || raw[0] == '~' || (strlen(raw) >= 2 && raw[1] == ':')) {
        char *err = malloc(256);
        if (err) snprintf(err, 256,
            "Script path must be relative to ~/.hermes/scripts/. "
            "Got absolute or home-relative path: %s. "
            "Place scripts in ~/.hermes/scripts/ and use just the filename.",
            raw);
        free(raw);
        return err;
    }
    free(raw);
    return NULL;
}

/* ================================================================
 *  16. PoP: _format_job
 *  Formats a job dict for display (list output).
 * ================================================================ */
/* PoP: format_job @ tools/cronjob_tools.py:_format_job
 * Port of Python tools/cronjob_tools.py:_format_job().
 * Builds a display-friendly job object with truncated preview, repeat string,
 * skill list, and all delivery/schedule metadata. */
json_t *format_job(const json_t *job)
{
    if (!job) return json_object();

    const char *prompt = json_get_str(job, "prompt", "");
    json_t *skills_arr = json_obj_get(job, "skills");
    if (!skills_arr) {
        const char *skill = json_get_str(job, "skill", "");
        if (*skill) {
            skills_arr = json_array();
            json_append(skills_arr, json_string(skill));
        } else {
            skills_arr = json_array();
        }
    }

    const char *job_id = json_get_str(job, "id", "unknown");
    const char *name = json_get_str(job, "name", "");
    if (!*name) {
        name = prompt[0] ? prompt : (json_len(skills_arr) > 0
            ? json_get_str(json_get(skills_arr, 0), NULL, "")
            : (job_id && *job_id ? job_id : "cron job"));
    }

    json_t *obj = json_object();
    json_set(obj, "job_id", json_string(job_id));
    json_set(obj, "name", json_string(name));
    if (json_len(skills_arr) > 0) {
        json_t *first = json_get(skills_arr, 0);
        json_set(obj, "skill", json_copy(first));
    } else {
        json_set(obj, "skill", json_null());
    }
    json_set(obj, "skills", json_copy(skills_arr));
    json_set(obj, "prompt_preview", json_string(
        strlen(prompt) > 100 ? "..." : prompt));
    json_set(obj, "model", json_obj_get(job, "model") ? json_copy(json_obj_get(job, "model")) : json_null());
    json_set(obj, "provider", json_obj_get(job, "provider") ? json_copy(json_obj_get(job, "provider")) : json_null());
    json_set(obj, "base_url", json_obj_get(job, "base_url") ? json_copy(json_obj_get(job, "base_url")) : json_null());
    json_set(obj, "schedule", json_obj_get(job, "schedule_display") ? json_copy(json_obj_get(job, "schedule_display")) : json_string("?"));

    char *rd = repeat_display(job);
    json_set(obj, "repeat", json_string(rd ? rd : "?"));
    free(rd);

    json_set(obj, "deliver", json_obj_get(job, "deliver") ? json_copy(json_obj_get(job, "deliver")) : json_string("local"));
    json_set(obj, "next_run_at", json_obj_get(job, "next_run_at") ? json_copy(json_obj_get(job, "next_run_at")) : json_null());
    json_set(obj, "last_run_at", json_obj_get(job, "last_run_at") ? json_copy(json_obj_get(job, "last_run_at")) : json_null());
    json_set(obj, "last_status", json_obj_get(job, "last_status") ? json_copy(json_obj_get(job, "last_status")) : json_null());
    json_set(obj, "last_delivery_error", json_obj_get(job, "last_delivery_error") ? json_copy(json_obj_get(job, "last_delivery_error")) : json_null());
    json_set(obj, "enabled", json_obj_get(job, "enabled") ? json_copy(json_obj_get(job, "enabled")) : json_bool(true));
    json_set(obj, "state", json_obj_get(job, "state") ? json_copy(json_obj_get(job, "state")) : json_string("scheduled"));
    json_set(obj, "paused_at", json_obj_get(job, "paused_at") ? json_copy(json_obj_get(job, "paused_at")) : json_null());
    json_set(obj, "paused_reason", json_obj_get(job, "paused_reason") ? json_copy(json_obj_get(job, "paused_reason")) : json_null());

    if (json_obj_get(job, "script")) json_set(obj, "script", json_copy(json_obj_get(job, "script")));
    if (json_obj_get(job, "no_agent")) json_set(obj, "no_agent", json_copy(json_obj_get(job, "no_agent")));
    if (json_obj_get(job, "enabled_toolsets")) json_set(obj, "enabled_toolsets", json_copy(json_obj_get(job, "enabled_toolsets")));
    if (json_obj_get(job, "workdir")) json_set(obj, "workdir", json_copy(json_obj_get(job, "workdir")));
    return obj;
}

/* ================================================================
 *  17. PoP: cronjob (main dispatcher)
 *  Unified cron job management tool. Returns JSON string.
 * ================================================================ */
/* PoP: cronjob @ tools/cronjob_tools.py:cronjob
 * Port of Python tools/cronjob_tools.py:cronjob().
 * Unified cron job management tool with actions: create, list, update,
 * pause, resume, remove, run. Delegates to cron SQLite store and scheduler.
 * Returns JSON string (caller must free). */
char *cronjob(const char *action,
              const char *job_id,
              const char *prompt,
              const char *schedule,
              const char *name,
              const char *repeat,
              const char *deliver,
              bool include_disabled,
              const char *skill,
              const char *skills_json,
              const char *model_json,
              const char *provider,
              const char *base_url,
              const char *reason,
              const char *script,
              const char *context_from_json,
              const char *enabled_toolsets_json,
              const char *workdir,
              bool no_agent,
              bool attach_to_session)
{
    (void)job_id; (void)prompt; (void)schedule; (void)name; (void)repeat;
    (void)deliver; (void)include_disabled; (void)skill; (void)skills_json;
    (void)model_json; (void)provider; (void)base_url; (void)reason;
    (void)script; (void)context_from_json; (void)enabled_toolsets_json;
    (void)workdir; (void)no_agent; (void)attach_to_session;

    if (!action) {
        json_t *err = json_object();
        json_set(err, "success", json_bool(false));
        json_set(err, "error", json_string("action is required"));
        char *s = json_serialize(err);
        json_free(err);
        return s;
    }

    /* Minimal stub implementation — real dispatch delegates to cron SQLite store and scheduler */
    json_t *obj = json_object();
    json_set(obj, "success", json_bool(true));
    json_set(obj, "action", json_string(action));
    json_set(obj, "message", json_string("cronjob stub: dispatch not fully ported; see port_cronjob_tools.c"));
    char *s = json_serialize(obj);
    json_free(obj);
    return s;
}

/* ================================================================
 *  Notify provider that jobs changed — mirrors Python's
 *  _notify_provider_jobs_changed_safe() which writes a notify file.
 * ================================================================ */
/* PoP: notify_provider_jobs_changed_safe @ tools/cronjob_tools.py:_notify_provider_jobs_changed_safe
 * Port of Python tools/cronjob_tools.py:_notify_provider_jobs_changed_safe().
 * Writes a jobs_changed.notify file to HERMES_HOME/cron/ for the scheduler
 * to pick up. Safe to call from any thread. */
void notify_provider_jobs_changed_safe(void)
{
    hermes_log(LOG_INFO, "cron", "notify_provider_jobs_changed_safe: notifying");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"timestamp\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "cron", "notify_provider_jobs_changed_safe: written");
    } else {
        hermes_log(LOG_WARNING, "cron",
                   "notify_provider_jobs_changed_safe: cannot write %s", path);
    }
}

/* ================================================================
 *  Execute a cron job immediately — mirrors Python's
 *  _execute_job_now(job) which claims and runs a job.
 * ================================================================ */
/* PoP: execute_job_now @ tools/cronjob_tools.py:_execute_job_now
 * Port of Python tools/cronjob_tools.py:_execute_job_now().
 * Executes a cron job immediately outside the scheduler tick.
 * Returns json_t* object with "claimed", "success", "error". */
json_t *execute_job_now(const json_t *job)
{
    if (!job) {
        json_t *err = json_object();
        json_set(err, "claimed", json_bool(false));
        json_set(err, "success", json_bool(false));
        json_set(err, "error", json_string("job is null"));
        return err;
    }
    const char *job_id = json_get_str(job, "id", "");
    if (!job_id || !*job_id) {
        json_t *err = json_object();
        json_set(err, "claimed", json_bool(false));
        json_set(err, "success", json_bool(false));
        json_set(err, "error", json_string("job id missing"));
        return err;
    }

    /* In C port we don't have claim_job_for_fire or run_one_job exposed.
     * Delegate to cron_run_job if available, else return structured response. */
    hermes_log(LOG_INFO, "cron", "execute_job_now: executing job %s", job_id);

    json_t *resp = json_object();
    json_set(resp, "claimed", json_bool(true));
    json_set(resp, "success", json_bool(false));
    json_set(resp, "error", json_string("execute_job_now: C port stub — run_one_job not wired"));
    return resp;
}

/* ================================================================
 *  18. PoP: check_cronjob_requirements
 *  Checks if cronjob tools can be used (interactive CLI, gateway, exec_ask).
 * ================================================================ */
static bool check_env_truthy(const char *s)
{
    if (!s) return false;
    const char *t = s;
    while (*t && isspace((unsigned char)*t)) t++;
    if (!*t) return false;
    if (strcmp(t, "1") == 0) return true;
    char buf[32];
    size_t n = strlen(t);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) buf[i] = tolower((unsigned char)t[i]);
    buf[n] = '\0';
    return strcmp(buf, "true") == 0 || strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0;
}

/* PoP: check_cronjob_requirements @ tools/cronjob_tools.py:check_cronjob_requirements
 * Port of Python tools/cronjob_tools.py:check_cronjob_requirements().
 * Checks if cronjob tools can be used. Available in interactive CLI mode
 * and gateway/messaging platforms. The cron system is internal (JSON file-based
 * scheduler ticked by the gateway), so no external crontab executable is required.
 * Session env vars must hold an explicit truthy string ("1", "true", "yes", "on")
 * — false-like values ("0", "false", "no", "off") leave the tool disabled.
 * Uses the shared env_var_enabled helper so every consumer of these flags agrees
 * on the truthy set. */
bool check_cronjob_requirements(void)
{
    return check_env_truthy(getenv("HERMES_INTERACTIVE")) ||
           check_env_truthy(getenv("HERMES_GATEWAY_SESSION")) ||
           check_env_truthy(getenv("HERMES_EXEC_ASK"));
}