/* Slermes C port — agent/verify_hooks.py (pure helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <regex.h>

#define VERIFY_HOOKS_DEFAULT_MAX_VERIFY_NUDGES 3
static const char *VERIFY_HOOKS_CODING_VERIFY_GUIDANCE =
    "[Coding] Before you run tests/linters or call this done: if this is "
    "creative UI/visual work, hold off on tests and linters until the user says "
    "they like the result or you're about to commit. And before every commit, "
    "clean your work: keep it KISS/DRY, match the surrounding code style, and be "
    "elitist, shorthand, clever, concise, efficient, and elegant.";

static bool is_truthy_value(const char *value, bool def)
{
    if (!value) return def;
    if (value[0] == '\0') return def;
    char buf[64]; size_t n = 0;
    for (size_t i = 0; value[i] && n < sizeof(buf) - 1; i++)
        if (value[i] != ' ' && value[i] != '\t') buf[n++] = (char)tolower((unsigned char)value[i]);
    buf[n] = '\0';
    return strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
           strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0;
}

/* Minimal: caller passes the already-resolved agent config dict as a JSON
 * string (mirrors Python passing config={"agent": {...}}). Returns a malloc'd
 * raw value token (number, bool, or unquoted word, or string content) or NULL. */
static char *agent_cfg_get(const char *config_json, const char *key)
{
    if (!config_json) return NULL;
    char pat[64]; snprintf(pat, sizeof(pat), "\"agent\"[ \t]*:[ \t]*\\{");
    regex_t re; if (regcomp(&re, pat, REG_EXTENDED) != 0) return NULL;
    regmatch_t m;
    if (regexec(&re, config_json, 1, &m, 0) != 0) { regfree(&re); return NULL; }
    char *block = (char *)config_json + m.rm_eo;
    int depth = 1; size_t i = 0; char *end = NULL;
    while (block[i]) { if (block[i]=='{') depth++; else if (block[i]=='}'){depth--; if(depth==0){end=block+i; break;}} i++; }
    if (!end) { regfree(&re); return NULL; }
    size_t blen = (size_t)(end - block);
    char *bbuf = malloc(blen + 1); memcpy(bbuf, block, blen); bbuf[blen] = '\0';
    snprintf(pat, sizeof(pat), "\"%s\"[ \t]*:[ \t]*", key);
    regex_t re2; int ok = (regcomp(&re2, pat, REG_EXTENDED) == 0);
    char *result = NULL;
    if (ok && regexec(&re2, bbuf, 1, &m, 0) == 0) {
        char *s = bbuf + m.rm_eo;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '"') {
            size_t o = 0; char *val = malloc(256);
            while (s[1] && s[1] != '"' && o < 255) {
                if (s[1] == '\\' && s[2]) { val[o++] = s[2]; s += 2; } else { val[o++] = s[1]; s++; }
            }
            val[o] = '\0'; result = val;
        } else {
            /* number / bool / word: read until , or } */
            size_t o = 0; char *val = malloc(64);
            while (*s && *s != ',' && *s != '}' && o < 63) {
                if (*s == ' ' || *s == '\t') { s++; continue; }
                val[o++] = *s++;
            }
            val[o] = '\0'; result = val;
        }
    }
    if (ok) regfree(&re2);
    regfree(&re);
    free(bbuf);
    return result;
}

/* PoP: agent_verify_hooks_max_verify_nudges @ agent/verify_hooks.py:max_verify_nudges */
int agent_verify_hooks_max_verify_nudges(const char *config_json)
{
    char *raw = agent_cfg_get(config_json, "max_verify_nudges");
    if (!raw) return VERIFY_HOOKS_DEFAULT_MAX_VERIFY_NUDGES;
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    int ret = (end == raw) ? VERIFY_HOOKS_DEFAULT_MAX_VERIFY_NUDGES : (v < 0 ? 0 : (int)v);
    free(raw);
    return ret;
}

/* PoP: agent_verify_hooks_coding_verify_guidance @ agent/verify_hooks.py:coding_verify_guidance */
const char *agent_verify_hooks_coding_verify_guidance(const char *config_json)
{
    const char *raw = agent_cfg_get(config_json, "verify_guidance");
    if (!raw) return VERIFY_HOOKS_CODING_VERIFY_GUIDANCE; /* default True */
    if (!is_truthy_value(raw, true)) return NULL;
    return VERIFY_HOOKS_CODING_VERIFY_GUIDANCE;
}
