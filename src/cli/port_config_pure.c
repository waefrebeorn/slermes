/*
 * port_config_pure.c
 *
 * Pure, portable helper functions ported from hermes_cli/config.py and
 * hermes_cli/models.py. These contain no file I/O, no networking, and no
 * os.* / platform calls — only JSON-tree manipulation and small constant
 * table lookups. Each function is a faithful, self-contained port of its
 * Python counterpart.
 *
 * Functions:
 *   config_deep_merge                       <- config.py:_deep_merge
 *   config_items_by_unique_name             <- config.py:_items_by_unique_name
 *   config_normalize_max_turns              <- config.py:_normalize_max_turns_config
 *   config_strip_non_ascii_credential       <- config.py:_check_non_ascii_credential
 *   provider_group_for_slug                 <- models.py:provider_group_for_slug
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/* ===========================================================================
 *  _deep_merge  (config.py:_deep_merge)
 *  Recursively merge *override* into a copy of *base*; nested dicts recurse,
 *  scalar/list overrides replace. Returns a NEW json object (caller frees).
 * =========================================================================== */

json_t *config_deep_merge(const json_t *base, const json_t *override);

/* PoP: config_deep_merge @ hermes_cli/config.py:_deep_merge */
json_t *config_deep_merge(const json_t *base, const json_t *override)
{
    /* result = deep copy of base so the caller's trees stay owned by caller */
    json_t *result = json_copy(base);
    if (!result) return NULL;

    if (!override || override->type != JSON_OBJECT) return result;

    size_t n = json_object_size(override);
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(override, i);
        json_t *val = json_object_get_at(override, i);
        if (!key || !val) continue;

        json_t *existing = json_obj_get(result, key);
        if (existing && existing->type == JSON_OBJECT &&
            val->type == JSON_OBJECT) {
            /* recurse; merged is a NEW object owned by json_set below */
            json_t *merged = config_deep_merge(existing, val);
            json_set(result, key, merged); /* takes ownership of merged */
        } else {
            /* replace: pass a COPY so override's tree is not consumed */
            json_set(result, key, json_copy(val));
        }
    }
    return result;
}

/* ===========================================================================
 *  _items_by_unique_name  (config.py:_items_by_unique_name)
 *  Return a name-indexed object only when every item is a dict with a unique
 *  string "name". Otherwise return NULL. Caller frees the returned object.
 * =========================================================================== */

/* PoP: config_items_by_unique_name @ hermes_cli/config.py:_items_by_unique_name */
json_t *config_items_by_unique_name(const json_t *items)
{
    if (!items || items->type != JSON_ARRAY) return NULL;

    json_t *indexed = json_object();
    if (!indexed) return NULL;

    size_t n = json_len(items);
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(items, i);
        if (!item || item->type != JSON_OBJECT) {
            json_free(indexed);
            return NULL;
        }
        json_t *name = json_obj_get(item, "name");
        if (!name || name->type != JSON_STRING ||
            !json_string_value(name) || !json_string_value(name)[0]) {
            json_free(indexed);
            return NULL;
        }
        const char *nm = json_string_value(name);
        if (json_obj_get(indexed, nm)) { /* duplicate name */
            json_free(indexed);
            return NULL;
        }
        json_set(indexed, nm, json_copy(item)); /* takes ownership */
    }
    return indexed;
}

/* ===========================================================================
 *  _normalize_max_turns_config  (config.py:_normalize_max_turns_config)
 *  Migrate legacy root-level max_turns into agent.max_turns. Inject the schema
 *  default (90) only when the user actually set max_turns somewhere; otherwise
 *  leave it absent. Returns a NEW object (caller frees) or NULL on bad input.
 * =========================================================================== */

/* PoP: config_normalize_max_turns @ hermes_cli/config.py:_normalize_max_turns_config */
json_t *config_normalize_max_turns(const json_t *config_in)
{
    if (!config_in || config_in->type != JSON_OBJECT) return NULL;

    /* DEFAULT_CONFIG["agent"]["max_turns"] == 90 */
    const double DEFAULT_MAX_TURNS = 90.0;

    json_t *config = json_copy(config_in);
    if (!config) return NULL;

    json_t *agent_cfg = json_obj_get(config, "agent");
    json_t *agent = (agent_cfg && agent_cfg->type == JSON_OBJECT)
                        ? json_copy(agent_cfg)
                        : json_object();
    if (!agent) { json_free(config); return NULL; }

    int had_root = (json_obj_get(config, "max_turns") != NULL);
    int had_agent = (json_obj_get(agent, "max_turns") != NULL);

    if (had_root && !had_agent) {
        json_t *mt = json_obj_get(config, "max_turns");
        json_set(agent, "max_turns", json_copy(mt));
    }
    /* Only inject the default when the user explicitly set max_turns
     * (either root-level or under agent). Otherwise leave it absent. */
    if (!had_root && !had_agent) {
        /* deliberately do not inject the default */
    } else if (json_obj_get(agent, "max_turns") == NULL) {
        json_set(agent, "max_turns", json_number(DEFAULT_MAX_TURNS));
    }

    /* Rebuild config without the root-level max_turns key (no remove helper),
     * then attach the migrated agent block. */
    json_t *out = json_object();
    if (!out) { json_free(agent); json_free(config); return NULL; }
    size_t n = json_object_size(config);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(config, i);
        json_t *v = json_object_get_at(config, i);
        if (!k || !v) continue;
        if (strcmp(k, "max_turns") == 0) continue; /* drop root key */
        json_set(out, k, json_copy(v));
    }
    json_set(out, "agent", agent); /* agent now owned by out */

    json_free(config);
    return out;
}

/* ===========================================================================
 *  _check_non_ascii_credential  (config.py:_check_non_ascii_credential)
 *  Strip non-ASCII characters from a credential value (API keys must be pure
 *  ASCII). Returns a malloc'd sanitized string (caller frees); when warn is
 *  non-NULL it receives a human-readable warning listing offending characters.
 * =========================================================================== */

static int has_non_ascii(const char *s)
{
    for (const char *p = s; *p; p++)
        if ((unsigned char)*p > 127) return 1;
    return 0;
}

/* PoP: config_strip_non_ascii_credential @ hermes_cli/config.py:_check_non_ascii_credential */
char *config_strip_non_ascii_credential(const char *key, const char *value,
                                        char *warn, size_t warnsz)
{
    if (!value) return NULL;

    if (!has_non_ascii(value))
        return strdup(value); /* all ASCII — nothing to do */

    /* Build the ASCII-only (drop) sanitized copy. */
    char *san = (char *)malloc(strlen(value) + 1);
    if (!san) return NULL;
    size_t j = 0;
    for (const char *p = value; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= 127) san[j++] = (char)c;
    }
    san[j] = '\0';

    if (warn && warnsz > 0) {
        warn[0] = '\0';
        snprintf(warn, warnsz,
            "\n  Warning: %s contains non-ASCII characters that will break API requests.\n"
            "  This usually happens when copy-pasting from a PDF, rich-text editor,\n"
            "  or web page that substitutes lookalike Unicode glyphs for ASCII letters.\n",
            key ? key : "credential");
        int shown = 0;
        for (const char *p = value; *p && shown < 5; p++) {
            unsigned char c = (unsigned char)*p;
            if (c > 127) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp),
                         "\n  position %ld: '%c' (U+%04X)",
                         (long)(p - value), c, (unsigned)c);
                size_t cur = strlen(warn);
                if (cur + strlen(tmp) + 1 < warnsz) strcat(warn, tmp);
                shown++;
            }
        }
        int total = 0;
        for (const char *p = value; *p; p++)
            if ((unsigned char)*p > 127) total++;
        if (total > 5) {
            size_t cur = strlen(warn);
            if (cur + strlen("\n  ... and more") + 1 < warnsz)
                strcat(warn, "\n  ... and more");
        }
        const char *tail =
            "\n\n  The non-ASCII characters have been stripped automatically.\n"
            "  If authentication fails, re-copy the key from the provider's dashboard.\n";
        size_t cur = strlen(warn);
        if (cur + strlen(tail) + 1 < warnsz)
            strcat(warn, tail);
    }

    return san;
}

/* ===========================================================================
 *  provider_group_for_slug  (models.py:provider_group_for_slug)
 *  Reverse index member-slug -> group_id, built once from the static
 *  PROVIDER_GROUPS table. Returns "" for ungrouped slugs.
 * =========================================================================== */

/* member slug -> group_id, expanded from PROVIDER_GROUPS in models.py */
static const char *SLUG_TO_GROUP[][2] = {
    {"kimi-coding",     "kimi"},
    {"kimi-coding-cn",  "kimi"},
    {"minimax",         "minimax"},
    {"minimax-oauth",   "minimax"},
    {"minimax-cn",      "minimax"},
    {"xai",             "xai"},
    {"xai-oauth",       "xai"},
    {"gemini",          "google"},
    {"openai-codex",    "openai"},
    {"openai-api",      "openai"},
    {"opencode-zen",    "opencode"},
    {"opencode-go",     "opencode"},
    {"copilot",         "copilot"},
    {"copilot-acp",     "copilot"},
    {NULL, NULL}
};

/*
 * PoP: provider_group_for_slug @ hermes_cli/models.py:provider_group_for_slug
 * Return the group_id a provider slug belongs to, or "" if ungrouped.
 */
const char *provider_group_for_slug(const char *slug)
{
    if (!slug) return "";
    char buf[128];
    size_t i;
    for (i = 0; slug[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)slug[i]);
    buf[i] = '\0';
    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = '\0';

    for (int k = 0; SLUG_TO_GROUP[k][0]; k++)
        if (strcmp(p, SLUG_TO_GROUP[k][0]) == 0)
            return SLUG_TO_GROUP[k][1];
    return "";
}
