/*
 * threat_patterns.c — prompt-injection / exfiltration pattern detection.
 *
 * Port of tools/threat_patterns.py. Matchers are case-insensitive substring
 * tests on a lowercased copy of the input; each pattern is a conjunction of
 * required fragments plus optional fragments, which reproduces the intent of
 * the Python regexes without a regex engine.
 *
 * Pattern order is significant: the first match in table order decides
 * `pattern_id`, matching the Python module's first-match-wins behaviour.
 */

#include "threat_patterns.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

typedef struct {
    const char *id;
    int scope;
    /* All of these must appear (lowercase literals). */
    const char *const *all_of;
    size_t all_n;
    /* At least one of these must appear when any_of_n > 0. */
    const char *const *any_of;
    size_t any_n;
} pattern_t;

static const char *ALL_IGNORE_PREV[] = {
    "ignore all previous instructions",
    "ignore previous instructions",
    "ignore all prior instructions",
    "ignore prior instructions",
};
static const char *ALL_SYS_OVERRIDE[] = {
    "system prompt override",
    "override the system prompt",
    "ignore the system prompt",
    "ignore your system prompt",
};
static const char *ALL_DISREGARD[] = {
    "disregard all your instructions",
    "disregard your instructions",
    "disregard all instructions",
    "disregard previous instructions",
};
static const char *ALL_TRANSLATE_ALL[] = { "translate" };
static const char *ALL_TRANSLATE_ANY[] = { "execute", "execut" };
static const char *ALL_CURL_ALL[] = { "curl", "$" };
static const char *ALL_WGET_ALL[] = { "wget", "$" };

static const char *CTX_ROLE_ALL[] = { "you are now" };
static const char *CTX_ROLE_ANY[] = { "manager", "free ai", "no restrictions", "unrestricted" };
static const char *CTX_IDENT_ALL[] = { "yourself" };
static const char *CTX_IDENT_ANY[] = { "name ", "call ", "rename " };
static const char *CTX_C2NODE_ALL[] = { "register as a node" };
static const char *CTX_C2NODE_ANY[] = { "command and control", "c2" };
static const char *CTX_C2PULL_ALL[] = { "pull" };
static const char *CTX_C2PULL_ANY[] = { "new tasks", "down tasks" };
static const char *CTX_C2NET_ALL[] = { "connect to the network" };

static const char *STRICT_SECRET_ALL[] = { "api_key =" };
static const char *STRICT_SECRET_ANY[] = { "sk-", "\"sk", "secret" };
static const char *STRICT_SENDURL_ALL[] = { "send the data to" };
static const char *STRICT_SENDURL_ANY[] = { "http", "url", "server" };

#define N(a) (sizeof(a) / sizeof((a)[0]))

static const pattern_t TABLE[] = {
    /* These three list mutually exclusive phrasings, so they are any-of sets:
     * treating them as all-of demanded every phrasing at once. */
    {"prompt_injection",    THREAT_SCOPE_ALL,     NULL, 0, ALL_IGNORE_PREV,   N(ALL_IGNORE_PREV)},
    {"sys_prompt_override", THREAT_SCOPE_ALL,     NULL, 0, ALL_SYS_OVERRIDE,  N(ALL_SYS_OVERRIDE)},
    {"disregard_rules",     THREAT_SCOPE_ALL,     NULL, 0, ALL_DISREGARD,     N(ALL_DISREGARD)},
    {"translate_execute",   THREAT_SCOPE_ALL,     ALL_TRANSLATE_ALL,  N(ALL_TRANSLATE_ALL),  ALL_TRANSLATE_ANY, N(ALL_TRANSLATE_ANY)},
    {"exfil_curl",          THREAT_SCOPE_ALL,     ALL_CURL_ALL,       N(ALL_CURL_ALL),       NULL, 0},
    {"exfil_wget",          THREAT_SCOPE_ALL,     ALL_WGET_ALL,       N(ALL_WGET_ALL),       NULL, 0},

    {"role_hijack",         THREAT_SCOPE_CONTEXT, CTX_ROLE_ALL,       N(CTX_ROLE_ALL),       CTX_ROLE_ANY,   N(CTX_ROLE_ANY)},
    {"identity_override",   THREAT_SCOPE_CONTEXT, CTX_IDENT_ALL,      N(CTX_IDENT_ALL),      CTX_IDENT_ANY,  N(CTX_IDENT_ANY)},
    {"c2_node_registration",THREAT_SCOPE_CONTEXT, CTX_C2NODE_ALL,     N(CTX_C2NODE_ALL),     CTX_C2NODE_ANY, N(CTX_C2NODE_ANY)},
    {"c2_task_pull",        THREAT_SCOPE_CONTEXT, CTX_C2PULL_ALL,     N(CTX_C2PULL_ALL),     CTX_C2PULL_ANY, N(CTX_C2PULL_ANY)},
    {"c2_network_connect",  THREAT_SCOPE_CONTEXT, CTX_C2NET_ALL,      N(CTX_C2NET_ALL),      NULL, 0},

    {"hardcoded_secret",    THREAT_SCOPE_STRICT,  STRICT_SECRET_ALL,  N(STRICT_SECRET_ALL),  STRICT_SECRET_ANY, N(STRICT_SECRET_ANY)},
    {"send_to_url",         THREAT_SCOPE_STRICT,  STRICT_SENDURL_ALL, N(STRICT_SENDURL_ALL), STRICT_SENDURL_ANY, N(STRICT_SENDURL_ANY)},
};

#define TABLE_N (sizeof(TABLE) / sizeof(TABLE[0]))

static int g_initialised = 0;

static char *lower_dup(const char *s)
{
    size_t n = strlen(s);
    char *b = malloc(n + 1);
    if (!b) return NULL;
    for (size_t i = 0; i < n; i++) b[i] = (char)tolower((unsigned char)s[i]);
    b[n] = '\0';
    return b;
}

static const char *find_any(const char *hay, const char *const *needles, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const char *p = strstr(hay, needles[i]);
        if (p) return p;
    }
    return NULL;
}

static int matches(const pattern_t *pat, const char *hay, const char **where)
{
    for (size_t i = 0; i < pat->all_n; i++) {
        const char *p = strstr(hay, pat->all_of[i]);
        if (!p) return 0;
        if (where) *where = p;
    }
    if (pat->any_n) {
        const char *p = find_any(hay, pat->any_of, pat->any_n);
        if (!p) return 0;
        if (where) *where = p;
    }
    return 1;
}

/* PoP: threat_patterns_init @ tools/threat_patterns.py:_compile_patterns */
int threat_patterns_init(void)
{
    g_initialised = 1;
    return (int)TABLE_N;
}

/* PoP: threat_patterns_count @ tools/threat_patterns.py:_compiled_patterns */
int threat_patterns_count(void)
{
    return g_initialised ? (int)TABLE_N : 0;
}

/* PoP: threat_patterns_cleanup @ tools/threat_patterns.py:_cleanup */
void threat_patterns_cleanup(void)
{
    g_initialised = 0;
}

/* PoP: threat_patterns_check @ tools/threat_patterns.py:check_for_threats */
bool threat_patterns_check(const char *text, unsigned scope_mask,
                           threat_match_t *out)
{
    if (!text || !text[0]) return false;
    if (!g_initialised) threat_patterns_init();

    char *hay = lower_dup(text);
    if (!hay) return false;

    bool hit = false;
    for (size_t i = 0; i < TABLE_N; i++) {
        const pattern_t *pat = &TABLE[i];
        if (!(scope_mask & (1u << (unsigned)pat->scope))) continue;

        const char *where = NULL;
        if (!matches(pat, hay, &where)) continue;

        if (out) {
            snprintf(out->pattern_id, sizeof(out->pattern_id), "%s", pat->id);
            out->scope = pat->scope;
            out->match_start = where ? (int)(where - hay) : -1;
            out->match_len = where ? (int)strlen(where) : 0;
        }
        hit = true;
        break;
    }

    free(hay);
    return hit;
}

/* PoP: threat_patterns_check_all @ tools/threat_patterns.py:check_all */
bool threat_patterns_check_all(const char *text, threat_match_t *out)
{
    unsigned mask = (1u << THREAT_SCOPE_ALL) |
                    (1u << THREAT_SCOPE_CONTEXT) |
                    (1u << THREAT_SCOPE_STRICT);
    return threat_patterns_check(text, mask, out);
}
