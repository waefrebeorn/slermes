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

/* PoP: deterministic_call_id @ agent/message_sanitization.py:deterministic_call_id */
/* Returns "call_" + first 12 hex chars of sha256("fn:args:index"). */
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

/* PoP: apply_reasoning_content_policy @ agent/message_sanitization.py:apply_reasoning_content_policy */
/* Mutates api_msg JSON object in place following the 5-case policy. */
int msg_sanitize_apply_reasoning_content_policy(json_t *source_msg, json_t *api_msg, int needs_thinking_pad)
{
    if (!source_msg || !api_msg || source_msg->type != JSON_OBJECT || api_msg->type != JSON_OBJECT)
        return 0;

    const char *role = json_get_str(source_msg, "role", "");
    if (strcmp(role, "assistant") != 0)
        return 0;

    json_t *existing = json_obj_get(source_msg, "reasoning_content");
    int changed = 0;

    if (existing && existing->type == JSON_STRING) {
        const char *val = existing->str_val;
        if (!needs_thinking_pad) {
            if (json_obj_get(api_msg, "reasoning_content")) {
                json_obj_del(api_msg, "reasoning_content");
                changed = 1;
            }
        } else if (val && strcmp(val, "") == 0) {
            json_set(api_msg, "reasoning_content", json_string(" "));
            changed = 1;
        } else {
            json_set(api_msg, "reasoning_content", json_string(val));
            changed = 1;
        }
        return changed;
    }

    /* Case 2: cross-provider poisoned history */
    json_t *reasoning = json_obj_get(source_msg, "reasoning");
    json_t *tool_calls = json_obj_get(source_msg, "tool_calls");
    if (needs_thinking_pad && tool_calls && tool_calls->type == JSON_ARRAY &&
        tool_calls->c.count > 0 && reasoning && reasoning->type == JSON_STRING &&
        reasoning->str_val && *reasoning->str_val) {
        json_set(api_msg, "reasoning_content", json_string(" "));
        return 1;
    }

    /* Case 3: promote 'reasoning' field to 'reasoning_content' */
    if (reasoning && reasoning->type == JSON_STRING && reasoning->str_val && *reasoning->str_val) {
        if (needs_thinking_pad) {
            json_set(api_msg, "reasoning_content", json_string(reasoning->str_val));
            changed = 1;
        } else {
            if (json_obj_get(api_msg, "reasoning_content")) {
                json_obj_del(api_msg, "reasoning_content");
                changed = 1;
            }
        }
        return changed;
    }

    /* Case 4: inject single space for thinking-mode providers */
    if (needs_thinking_pad) {
        json_set(api_msg, "reasoning_content", json_string(" "));
        return 1;
    }

    /* Case 5: not a string (e.g. None after compaction) */
    if (json_obj_get(api_msg, "reasoning_content")) {
        json_obj_del(api_msg, "reasoning_content");
        changed = 1;
    }
    return changed;
}

/* PoP: reapply_reasoning_echo @ agent/message_sanitization.py:reapply_reasoning_echo */
int msg_sanitize_reapply_reasoning_echo(json_t *api_messages, int needs_thinking_pad)
{
    if (!api_messages || api_messages->type != JSON_ARRAY)
        return 0;

    int changed = 0;
    for (size_t i = 0; i < api_messages->c.count; i++) {
        json_t *msg = json_get(api_messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "assistant") != 0) continue;

        if (needs_thinking_pad) {
            const char *rc = json_get_str(msg, "reasoning_content", "");
            if (rc && *rc) continue;  /* already has reasoning_content */
            msg_sanitize_apply_reasoning_content_policy(msg, msg, needs_thinking_pad);
            const char *now = json_get_str(msg, "reasoning_content", "");
            if (now && *now) changed++;
        } else {
            if (json_obj_get(msg, "reasoning_content")) {
                json_obj_del(msg, "reasoning_content");
                changed++;
            }
        }
    }
    return changed;
}

/* PoP: uniquify_tool_call_ids @ agent/message_sanitization.py:uniquify_tool_call_ids */
/* Mutates tool_calls array in place (dicts get _d<n> suffixes on collision). */
json_t *msg_sanitize_uniquify_tool_call_ids(json_t *tool_calls)
{
    if (!tool_calls || tool_calls->type != JSON_ARRAY)
        return tool_calls;

    /* Collect seen IDs (simple linked list of strdup'd strings). */
    char *seen[256];
    size_t nseen = 0;

    for (size_t i = 0; i < tool_calls->c.count; i++) {
        json_t *tc = json_get(tool_calls, i);
        if (!tc || tc->type != JSON_OBJECT) continue;

        /* Extract raw id */
        const char *raw = NULL;
        json_t *call_id = json_obj_get(tc, "call_id");
        json_t *id = json_obj_get(tc, "id");
        if (call_id && call_id->type == JSON_STRING && call_id->str_val)
            raw = call_id->str_val;
        else if (id && id->type == JSON_STRING && id->str_val)
            raw = id->str_val;

        if (!raw || !*raw) continue;

        char *raw_copy = strdup(raw);
        /* strip */
        char *end = raw_copy + strlen(raw_copy);
        while (end > raw_copy && isspace((unsigned char)*(end-1))) *--end = '\0';
        char *start = raw_copy;
        while (*start && isspace((unsigned char)*start)) start++;
        memmove(raw_copy, start, strlen(start) + 1);

        if (!*raw_copy) { free(raw_copy); continue; }

        /* Composite id: split on | */
        char *cid = raw_copy;
        char *pipe = strchr(cid, '|');
        if (pipe) *pipe = '\0';  /* cid = first part */

        if (!*cid) { free(raw_copy); continue; }

        /* Check if cid is already seen */
        int found = 0;
        for (size_t s = 0; s < nseen; s++) {
            if (strcmp(cid, seen[s]) == 0) { found = 1; break; }
        }
        if (!found) {
            if (nseen < 256) seen[nseen++] = strdup(cid);
            free(raw_copy);
            continue;
        }

        /* Collision — generate _d<n> suffix, increment until unique */
        long n_suf = 2;
        char new_id[256];
        while (1) {
            snprintf(new_id, sizeof(new_id), "%s_d%ld", cid, n_suf);
            int hit = 0;
            for (size_t s = 0; s < nseen; s++) {
                if (strcmp(new_id, seen[s]) == 0) { hit = 1; break; }
            }
            if (!hit) break;
            n_suf++;
        }

        if (nseen < 256) seen[nseen++] = strdup(new_id);

        /* Apply rename — preserve composite suffix */
        char *renamed = strdup(new_id);
        if (pipe) {
            /* Reconstruct: new_id|second_part */
            char *recon = malloc(strlen(new_id) + 1 + strlen(pipe + 1) + 1);
            sprintf(recon, "%s|%s", new_id, pipe + 1);
            free(renamed);
            renamed = recon;
        }

        /* Set on tc */
        if (id) {
            json_set(tc, "id", json_string(renamed));
        } else {
            json_set(tc, "id", json_string(renamed));
        }
        if (call_id) {
            json_set(tc, "call_id", json_string(new_id));
        }

        free(renamed);
        free(raw_copy);
    }

    for (size_t s = 0; s < nseen; s++) free(seen[s]);
    return tool_calls;
}
