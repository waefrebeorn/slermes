/* Faithful C11 ports of cli.py leaf functions.
 * Each function reads the Python source and implements real logic.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── _load_prefill_messages ─────────────────────────────────────────── */
/* PoP: _load_prefill_messages @ cli.py:_load_prefill_messages */
json_t *_load_prefill_messages(const char *file_path) {
    if (!file_path || !*file_path) return NULL;

    /* Resolve path: expand ~ and make absolute relative to ~/.hermes */
    char resolved[2048];
    if (file_path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) return NULL;
        snprintf(resolved, sizeof(resolved), "%s%s", home, file_path + 1);
    } else if (file_path[0] != '/') {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) return NULL;
        snprintf(resolved, sizeof(resolved), "%s/%s", home, file_path);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", file_path);
    }

    /* Parse JSON file directly */
    char *err = NULL;
    json_t *data = json_parse_file(resolved, &err);
    if (!data) return NULL;

    /* Must be an array */
    if (!json_is_array(data)) {
        json_free(data);
        return NULL;
    }

    return data;
}

/* ── _resolve_prefill_messages_file ─────────────────────────────────── */
/* PoP: _resolve_prefill_messages_file @ cli.py:_resolve_prefill_messages_file */
char *_resolve_prefill_messages_file(json_t *config) {
    /* Returns a malloc'd string (caller must free). */

    /* 1. Environment variable wins */
    const char *env_path = getenv("HERMES_PREFILL_MESSAGES_FILE");
    if (env_path && *env_path) {
        return strdup(env_path);
    }

    /* 2. Top-level config key */
    if (config && json_is_object(config)) {
        json_t *top = json_object_get(config, "prefill_messages_file");
        if (top && json_is_string(top)) {
            const char *s = json_get_str(top, NULL, NULL);
            if (s && *s) return strdup(s);

        /* 3. Legacy agent.prefill_messages_file fallback */
        json_t *agent = json_object_get(config, "agent");
        if (agent && json_is_object(agent)) {
            json_t *legacy = json_object_get(agent, "prefill_messages_file");
            if (legacy && json_is_string(legacy)) {
                const char *s = json_get_str(legacy, NULL, NULL);
                if (s && *s) return strdup(s);
            }
        }
        }
    }

    return strdup("");
}

/* ── _parse_service_tier_config ─────────────────────────────────────── */
/* PoP: _parse_service_tier_config @ cli.py:_parse_service_tier_config */
char *_parse_service_tier_config(const char *raw) {
    /* Returns a malloc'd string (caller must free) or NULL. */
    if (!raw) return NULL;

    /* Strip whitespace and lowercase */
    char *value = strdup(raw);
    if (!value) return NULL;
    /* trim */
    char *start = value;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    /* lowercase */
    for (char *p = start; *p; p++) *p = tolower((unsigned char)*p);

    char *result = NULL;
    if (!*start || strcmp(start, "normal") == 0 || strcmp(start, "default") == 0 ||
        strcmp(start, "standard") == 0 || strcmp(start, "off") == 0 ||
        strcmp(start, "none") == 0) {
        result = NULL;  /* no tier */
    } else if (strcmp(start, "fast") == 0 || strcmp(start, "priority") == 0 ||
               strcmp(start, "on") == 0) {
        result = strdup("priority");
    } else {
        result = NULL;  /* unknown, ignore */
    }

    free(value);
    return result;
}
