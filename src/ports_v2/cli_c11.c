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

/* ── _split_destructive_skip ────────────────────────────────────────── */
/* PoP: _split_destructive_skip @ cli.py:_split_destructive_skip */
/* Returns malloc'd remainder in *remainder, sets *skip to true if skip token found.
 * Caller must free(*remainder). */
void _split_destructive_skip(const char *cmd_text, char **remainder, bool *skip) {
    static const char *skip_tokens[] = {"now", "--yes", "-y", NULL};

    *skip = false;
    *remainder = strdup("");

    if (!cmd_text || !*cmd_text) return;

    /* Copy and tokenize */
    char *work = strdup(cmd_text);
    if (!work) return;

    /* Strip leading whitespace */
    char *p = work;
    while (*p == ' ' || *p == '\t') p++;

    /* Skip leading /cmd word */
    if (*p == '/') {
        while (*p && *p != ' ' && *p != '\t') p++;
        while (*p == ' ' || *p == '\t') p++;
    }

    /* Process tokens */
    char *out = (char*)malloc(strlen(p) + 1);
    if (!out) { free(work); return; }
    out[0] = '\0';

    char *token = strtok(p, " \t");
    while (token) {
        /* Check if token is a skip token */
        bool is_skip = false;
        for (int i = 0; skip_tokens[i]; i++) {
            if (strcasecmp(token, skip_tokens[i]) == 0) {
                is_skip = true;
                break;
            }
        }
        if (is_skip) {
            *skip = true;
        } else {
            if (out[0]) strcat(out, " ");
            strcat(out, token);
        }
        token = strtok(NULL, " \t");
    }

    free(work);
    *remainder = out;
}

/* ── _compute_model_picker_viewport ─────────────────────────────────── */
/* PoP: _compute_model_picker_viewport @ cli.py:_compute_model_picker_viewport */
void _compute_model_picker_viewport(int selected, int scroll_offset, int n,
                                     int term_rows, int reserved_below,
                                     int panel_chrome, int min_visible,
                                     int *out_scroll_offset, int *out_visible) {
    int max_visible = min_visible;
    int available = term_rows - reserved_below - panel_chrome;
    if (available > max_visible) max_visible = available;

    if (n <= max_visible) {
        *out_scroll_offset = 0;
        *out_visible = n;
        return;
    }

    int visible = max_visible;
    if (selected < scroll_offset) {
        scroll_offset = selected;
    } else if (selected >= scroll_offset + visible) {
        scroll_offset = selected - visible + 1;
    }
    int max_offset = n - visible;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset > max_offset) scroll_offset = max_offset;

    *out_scroll_offset = scroll_offset;
    *out_visible = visible;
}

/* ── _normalize_slash_confirm_choice ────────────────────────────────── */
/* PoP: _normalize_slash_confirm_choice @ cli.py:_normalize_slash_confirm_choice */
/* Returns malloc'd normalized choice or NULL. Caller must free. */
char *_normalize_slash_confirm_choice(const char *raw, const char **allowed, int n_choices) {
    if (!raw) return NULL;

    /* Strip and lowercase */
    while (*raw == ' ' || *raw == '\t') raw++;
    char *choice = strdup(raw);
    if (!choice) return NULL;
    for (char *p = choice; *p; p++) *p = tolower((unsigned char)*p);
    /* Trim trailing whitespace */
    char *end = choice + strlen(choice);
    while (end > choice && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';

    if (!choice[0]) { free(choice); return NULL; }

    /* Alias map */
    static const char *keys[] = {"1","once","approve","yes","y","ok","2","always","remember","3","cancel","nevermind","no","n",NULL};
    static const char *vals[] = {"once","once","once","once","once","once","always","always","always","cancel","cancel","cancel","cancel","cancel"};

    const char *normalized = NULL;
    for (int i = 0; keys[i]; i++) {
        if (strcmp(choice, keys[i]) == 0) {
            normalized = vals[i];
            break;
        }
    }
    if (!normalized) normalized = choice;  /* direct match */

    /* Check if normalized is in allowed set */
    for (int i = 0; i < n_choices; i++) {
        if (strcmp(normalized, allowed[i]) == 0) {
            char *result = strdup(normalized);
            free(choice);
            return result;
        }
    }

    free(choice);
    return NULL;
}

/* ── _resolve_checkpoint_ref ────────────────────────────────────────── */
/* PoP: _resolve_checkpoint_ref @ cli.py:_resolve_checkpoint_ref */
/* Resolves a checkpoint number (1-indexed) or hash string.
 * Returns malloc'd hash string or NULL on error. Caller must free. */
char *_resolve_checkpoint_ref(const char *ref, json_t *checkpoints) {
    if (!ref || !checkpoints) return NULL;

    /* Try parsing as a number */
    char *endptr = NULL;
    long idx = strtol(ref, &endptr, 10);
    if (endptr && *endptr == '\0' && idx >= 1) {
        /* 1-indexed: convert to 0-indexed */
        int i = (int)(idx - 1);
        int n = (int)json_len(checkpoints);
        if (i >= 0 && i < n) {
            json_t *entry = json_get(checkpoints, i);
            if (entry && json_is_object(entry)) {
                json_t *hash = json_object_get(entry, "hash");
                if (hash && json_is_string(hash)) {
                    return strdup(json_get_str(hash, NULL, NULL));
                }
            }
        }
        return NULL;
    }

    /* Not a number — treat as a git hash */
    return strdup(ref);
}

/* ── _approval_choices ──────────────────────────────────────────────── */
/* PoP: _approval_choices @ cli.py:_approval_choices */
/* Returns a NULL-terminated array of choice strings.
 * Caller must free the array (but not the strings — they're static). */
char **_approval_choices(const char *command, bool allow_permanent, bool smart_denied) {
    static const char *choices_smart[] = {"once", "deny", NULL};
    static const char *choices_full[] = {"once", "session", "always", "deny", NULL};
    static const char *choices_no_perm[] = {"once", "session", "deny", NULL};
    static const char *choices_full_view[] = {"once", "session", "always", "deny", "view", NULL};
    static const char *choices_no_perm_view[] = {"once", "session", "deny", "view", NULL};

    const char **src;
    if (smart_denied) {
        src = choices_smart;
    } else if (allow_permanent) {
        src = (command && strlen(command) > 70) ? choices_full_view : choices_full;
    } else {
        src = (command && strlen(command) > 70) ? choices_no_perm_view : choices_no_perm;
    }

    /* Count and copy */
    int n = 0;
    while (src[n]) n++;
    char **result = (char**)malloc((n + 1) * sizeof(char*));
    for (int i = 0; i < n; i++) result[i] = (char*)src[i];
    result[n] = NULL;
    return result;
}
