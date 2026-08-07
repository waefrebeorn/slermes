/*
 * message_sanitization_pure.c — Pure helpers from agent/message_sanitization.py.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "libcrypto/crypto.h"

/* Reasoning echo rule table (Python _REASONING_ECHO_RULES).
 * Each entry: family, raw_providers, lowered_providers, model_subs, hosts. */
typedef struct {
    const char *family;
    const char **raw_providers;   /* exact-case set */
    const char **lowered_providers;
    const char **model_subs;      /* lowered substrings */
    const char **hosts;
} echo_rule_t;

static const char *FAM_KIMI_RAW[] = {"kimi-coding", "kimi-coding-cn", NULL};
static const char *FAM_EMPTY[] = {NULL};
static const char *FAM_DEEPSEEK_LOWER[] = {"deepseek", NULL};
static const char *FAM_DEEPSEEK_MODELS[] = {"deepseek", NULL};
static const char *FAM_DEEPSEEK_HOSTS[] = {"api.deepseek.com", NULL};
static const char *FAM_MIMO_LOWER[] = {"xiaomi", NULL};
static const char *FAM_MIMO_MODELS[] = {"mimo", NULL};
static const char *FAM_MIMO_HOSTS[] = {"api.xiaomimimo.com", "miaoxiaomi.com", NULL};

static const echo_rule_t ECHO_RULES[] = {
    {
        "kimi", FAM_KIMI_RAW, FAM_EMPTY,
        FAM_EMPTY,
        (const char *[]){"api.kimi.com", "moonshot.ai", "moonshot.cn", NULL}
    },
    {
        "deepseek", FAM_EMPTY, FAM_DEEPSEEK_LOWER,
        FAM_DEEPSEEK_MODELS, FAM_DEEPSEEK_HOSTS
    },
    {
        "mimo", FAM_EMPTY, FAM_MIMO_LOWER,
        FAM_MIMO_MODELS, FAM_MIMO_HOSTS
    },
};
static const size_t ECHO_RULES_COUNT = 3;

static bool str_in_set(const char *val, const char **set) {
    if (!val || !set) return false;
    for (size_t i = 0; set[i]; i++)
        if (strcmp(val, set[i]) == 0) return true;
    return false;
}

static bool str_lower_in_set(const char *val_lower, const char **set_lower) {
    if (!val_lower || !set_lower) return false;
    for (size_t i = 0; set_lower[i]; i++)
        if (strcasecmp(val_lower, set_lower[i]) == 0) return true;
    return false;
}

static bool substr_in_list(const char *haystack_lower, const char **subs_lower) {
    if (!haystack_lower || !subs_lower) return false;
    for (size_t i = 0; subs_lower[i]; i++)
        if (strstr(haystack_lower, subs_lower[i])) return true;
    return false;
}

/* base_url_host_matches (from utils.py, inlined). */
static bool host_matches_any(const char *base_url, const char **hosts) {
    if (!base_url || !hosts) return false;
    /* Extract hostname */
    const char *p = base_url;
    const char *s = strstr(p, "://");
    if (s) p = s + 3;
    const char *slash = strchr(p, '/');
    size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
    char host[512];
    if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
    memcpy(host, p, hlen);
    host[hlen] = '\0';
    for (size_t i = 0; host[i]; i++)
        host[i] = (char)tolower((unsigned char)host[i]);
    size_t hl = strlen(host);
    while (hl > 0 && host[hl - 1] == '.') host[--hl] = '\0';
    if (!*host) return false;

    for (size_t i = 0; hosts[i]; i++) {
        const char *domain = hosts[i];
        if (strcmp(host, domain) == 0) return true;
        size_t dlen = strlen(domain);
        if (hl > dlen + 1 && host[hl - dlen - 1] == '.' &&
            strcmp(host + hl - dlen, domain) == 0) return true;
    }
    return false;
}

/* Helper: lowercase a string into a malloc'd buffer */
static char *to_lower(const char *s) {
    if (!s) return NULL;
    char *l = strdup(s);
    for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
    return l;
}

/* PoP: _family_rule @ agent/message_sanitization.py:_family_rule */
const echo_rule_t *msg_sanitize_family_rule(const char *family) {
    if (!family) return NULL;
    for (size_t i = 0; i < ECHO_RULES_COUNT; i++)
        if (strcmp(ECHO_RULES[i].family, family) == 0)
            return &ECHO_RULES[i];
    return NULL;
}

/* PoP: matches_reasoning_echo_family @ agent/message_sanitization.py:matches_reasoning_echo_family */
bool msg_sanitize_matches_reasoning_echo_family(
    const char *family, const char *provider, const char *model, const char *base_url
) {
    const echo_rule_t *rule = msg_sanitize_family_rule(family);
    if (!rule) return false;
    char *pl = to_lower(provider);
    char *ml = to_lower(model);
    bool ret = str_in_set(provider, rule->raw_providers) ||
               str_lower_in_set(pl, rule->lowered_providers) ||
               substr_in_list(ml, rule->model_subs) ||
               host_matches_any(base_url, rule->hosts);
    free(pl); free(ml);
    return ret;
}

/* PoP: reasoning_echo_family @ agent/message_sanitization.py:reasoning_echo_family */
const char *msg_sanitize_reasoning_echo_family(
    const char *provider, const char *model, const char *base_url
) {
    for (size_t i = 0; i < ECHO_RULES_COUNT; i++)
        if (msg_sanitize_matches_reasoning_echo_family(
                ECHO_RULES[i].family, provider, model, base_url))
            return ECHO_RULES[i].family;
    return NULL;
}

/* PoP: needs_reasoning_echo @ agent/message_sanitization.py:needs_reasoning_echo */
bool msg_sanitize_needs_reasoning_echo(
    const char *provider, const char *model, const char *base_url
) {
    return msg_sanitize_reasoning_echo_family(provider, model, base_url) != NULL;
}

/* PoP: deterministic_call_id @ agent/message_sanitization.py:deterministic_call_id
 * Returns "call_" + first 12 hex chars of sha256("fn_name:arguments:index"). */
char *msg_sanitize_deterministic_call_id(const char *fn_name,
                                          const char *arguments,
                                          int index) {
    char seed[2048];
    snprintf(seed, sizeof(seed), "%s:%s:%d",
             fn_name ? fn_name : "", arguments ? arguments : "", index);
    unsigned char hash[32];
    crypto_sha256((const unsigned char *)seed, strlen(seed), hash);
    static const char hexc[] = "0123456789abcdef";
    char *out = malloc(18);
    memcpy(out, "call_", 5);
    for (int i = 0; i < 6; i++) {
        out[5 + i * 2]     = hexc[hash[i] >> 4];
        out[5 + i * 2 + 1] = hexc[hash[i] & 0xF];
    }
    out[17] = '\0';
    return out;
}

/* PoP: coalesce_tool_call_id @ agent/message_sanitization.py:coalesce_tool_call_id */
const char *msg_sanitize_coalesce_tool_call_id(const json_t *tc) {
    if (!tc) return "";
    if (tc->type == JSON_OBJECT) {
        json_t *call_id = json_obj_get(tc, "call_id");
        if (call_id && call_id->type == JSON_STRING && call_id->str_val && *call_id->str_val) {
            char *s = strdup(call_id->str_val);
            /* strip */
            char *end = s + strlen(s) - 1;
            while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
            char *start = s;
            while (*start && isspace((unsigned char)*start)) start++;
            memmove(s, start, strlen(start) + 1);
            if (*s) return s;
            free(s);
        }
        json_t *id = json_obj_get(tc, "id");
        if (id && id->type == JSON_STRING && id->str_val && *id->str_val) {
            char *s = strdup(id->str_val);
            char *end = s + strlen(s) - 1;
            while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
            char *start = s;
            while (*start && isspace((unsigned char)*start)) start++;
            memmove(s, start, strlen(start) + 1);
            if (*s) return s;
            free(s);
        }
    }
    return "";
}
