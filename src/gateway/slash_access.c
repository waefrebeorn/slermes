/*
 * slash_access.c — Per-platform slash command access control.
 *
 * Port of Python gateway/slash_access.py.
 *
 * This module sits beside the existing per-platform allowlist (allow_from)
 * and adds a second axis: of the users who are allowed to talk to the
 * gateway, which ones can run which slash commands.
 *
 * Two lists per platform scope (DM vs group):
 *   - allow_admin_from      — user IDs that get every registered slash command
 *   - user_allowed_commands — slash command names non-admin users may run
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_slash_access.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Slash commands that MUST stay reachable for any allowed user */
static const char *ALWAYS_ALLOWED[] = {"help", "whoami", NULL};

/* DM chat type keywords */
static const char *_DM_CHAT_TYPES[] = {"dm", "direct", "private", "", NULL};

/* ================================================================
 *  SlashAccessPolicy structure and helpers
 *  Port of Python SlashAccessPolicy dataclass
 * ================================================================ */

/* (slash_policy_t defined in hermes_gateway.h) */

/* ================================================================
 *  Internal: check if a user_id is in comma-separated list
 * ================================================================ */

static bool id_in_list(const char *list, const char *user_id) {
    if (!list || !*list || !user_id || !*user_id) return false;

    /* Tokenize on commas */
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", list);
    char *save = NULL;
    char *tok = strtok_r(buf, ",", &save);
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (strcmp(tok, user_id) == 0) return true;
        tok = strtok_r(NULL, ",", &save);
    }
    return false;
}

/* PoP: is_admin @ gateway/slash_access.py:is_admin */
/* ================================================================
 *  Check if user is admin
 *  Port of Python SlashAccessPolicy.is_admin()
 * ================================================================ */

bool slash_policy_is_admin(const slash_policy_t *policy, const char *user_id) {
    if (!policy) return true; /* no policy = no gating */
    if (!policy->enabled) return true; /* gating disabled = everyone is admin */
    if (!user_id || !*user_id) return false;
    return id_in_list(policy->admin_user_ids, user_id);
}

/* PoP: can_run @ gateway/slash_access.py:can_run */
/* ================================================================
 *  Check if user can run a specific slash command
 *  Port of Python SlashAccessPolicy.can_run()
 * ================================================================ */

bool slash_policy_can_run(const slash_policy_t *policy,
                           const char *user_id,
                           const char *canonical_cmd) {
    if (!policy || !policy->enabled) return true;
    if (slash_policy_is_admin(policy, user_id)) return true;
    if (!canonical_cmd || !*canonical_cmd) return false;

    /* Check always-allowed commands */
    for (int i = 0; ALWAYS_ALLOWED[i]; i++) {
        if (strcasecmp(canonical_cmd, ALWAYS_ALLOWED[i]) == 0) return true;
    }

    return id_in_list(policy->user_allowed_commands, canonical_cmd);
}

/* ================================================================
 *  Normalize a YAML-loaded admin/user list into a comma-separated string
 *  Port of Python _coerce_id_list()
 *  AG26: Port of Python gateway/slash_access.py:_coerce_id_list().
 *
 *  Accepts a string (comma-separated) or JSON array.
 *  Returns malloc'd sorted comma-separated string, or "" on empty.
 * ================================================================ */

/* PoP: _coerce_id_list @ gateway/slash_access.py:_coerce_id_list */
static char *coerce_id_list(json_node_t *raw) {
    char buf[4096] = {0};
    size_t pos = 0;

    if (!raw) {
        return strdup("");
    }

    if (raw->type == JSON_ARRAY) {
        json_t *arr = (json_t *)raw;
        for (size_t i = 0; i < arr->c.count && pos < sizeof(buf) - 2; i++) {
            json_node_t *item = arr->c.items[i];
            if (!item) continue;
            const char *s = "";
            if (item->type == JSON_STRING) {
                s = item->str_val;
            } else if (item->type == JSON_NUMBER) {
                char num[64];
                snprintf(num, sizeof(num), "%.0f", item->num_val);
                s = num;
            }
            /* Strip whitespace */
            while (*s == ' ' || *s == '\t') s++;
            if (!*s) continue;
            if (pos > 0) buf[pos++] = ',';
            size_t slen = strlen(s);
            while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t')) slen--;
            size_t copy = slen < sizeof(buf) - pos - 1 ? slen : sizeof(buf) - pos - 1;
            memcpy(buf + pos, s, copy);
            pos += copy;
        }
    } else if (raw->type == JSON_STRING) {
        const char *s = raw->str_val;
        /* Split on comma */
        while (*s && pos < sizeof(buf) - 2) {
            while (*s == ' ' || *s == '\t') s++;
            if (!*s) break;
            if (pos > 0) buf[pos++] = ',';
            while (*s && *s != ',' && pos < sizeof(buf) - 2) {
                buf[pos++] = *s;
                s++;
            }
            if (*s == ',') s++;
        }
    } else if (raw->type == JSON_NUMBER) {
        char num[64];
        snprintf(num, sizeof(num), "%.0f", raw->num_val);
        snprintf(buf, sizeof(buf), "%s", num);
    }

    return strdup(buf);
}

/* ================================================================
 *  Normalize a slash command allowlist
 *  Port of Python _coerce_command_list()
 *  AG26: Port of Python gateway/slash_access.py:_coerce_command_list().
 *
 *  Strips leading slashes so config can list either "help" or "/help".
 *  Lowercase canonicalization. Returns malloc'd comma-separated string.
 * ================================================================ */

/* PoP: _coerce_command_list @ gateway/slash_access.py:_coerce_command_list */
static char *coerce_command_list(json_node_t *raw) {
    char buf[4096] = {0};
    size_t pos = 0;

    if (!raw) return strdup("");

    if (raw->type == JSON_ARRAY) {
        json_t *arr = (json_t *)raw;
        for (size_t i = 0; i < arr->c.count && pos < sizeof(buf) - 2; i++) {
            json_node_t *item = arr->c.items[i];
            if (!item || item->type != JSON_STRING) continue;
            const char *s = item->str_val;
            while (*s == ' ' || *s == '\t') s++;
            if (*s == '/') s++;
            if (!*s) continue;
            if (pos > 0) buf[pos++] = ',';
            while (*s && pos < sizeof(buf) - 2) {
                buf[pos++] = (char)tolower((unsigned char)*s);
                s++;
            }
        }
    } else if (raw->type == JSON_STRING) {
        const char *s = raw->str_val;
        while (*s && pos < sizeof(buf) - 2) {
            while (*s == ' ' || *s == '\t') s++;
            if (!*s) break;
            if (*s == '/') s++;
            if (pos > 0) buf[pos++] = ',';
            while (*s && *s != ',' && pos < sizeof(buf) - 2) {
                buf[pos++] = (char)tolower((unsigned char)*s);
                s++;
            }
            if (*s == ',') s++;
        }
    }

    return strdup(buf);
}

/* ================================================================
 *  Determine scope string from chat_type
 *  Port of Python _scope_for_chat_type()
 *  AG26: Port of Python gateway/slash_access.py:_scope_for_chat_type().
 * ================================================================ */

/* PoP: _scope_for_chat_type @ gateway/slash_access.py:_scope_for_chat_type */
static const char *scope_for_chat_type(const char *chat_type) {
    if (!chat_type || !*chat_type) return "dm";

    for (int i = 0; _DM_CHAT_TYPES[i]; i++) {
        if (strcasecmp(chat_type, _DM_CHAT_TYPES[i]) == 0)
            return "dm";
    }
    return "group";
}

/* ================================================================
 *  Get keys for a scope
 *  Port of Python _keys_for_scope()
 *  AG26: Port of Python gateway/slash_access.py:_keys_for_scope().
 * ================================================================ */

/* PoP: _keys_for_scope @ gateway/slash_access.py:_keys_for_scope */
static void keys_for_scope(const char *scope,
                            const char **admin_key,
                            const char **cmd_key) {
    if (scope && strcmp(scope, "group") == 0) {
        *admin_key = "group_allow_admin_from";
        *cmd_key = "group_user_allowed_commands";
    } else {
        *admin_key = "allow_admin_from";
        *cmd_key = "user_allowed_commands";
    }
}

/* ================================================================
 *  Return the "extra" dict from a gateway config entry
 *  Port of Python _platform_extra()
 *  AG26: Port of Python gateway/slash_access.py:_platform_extra().
 *
 *  Given a JSON node representing a platform config, extracts the
 *  "extra" sub-object. Defensively handles NULL and non-object shapes.
 * ================================================================ */

/* PoP: _platform_extra @ gateway/slash_access.py:_platform_extra */
static json_node_t *platform_extra(json_node_t *platform_config) {
    if (!platform_config) return json_new_object();
    if (platform_config->type != JSON_OBJECT)
        return json_copy(platform_config);

    json_node_t *extra = json_object_get(platform_config, "extra");
    if (!extra || extra->type != JSON_OBJECT) {
        return json_new_object();
    }
    return json_copy(extra);
}

/* ================================================================
 *  Build a policy from extra dict for one scope
 *  Port of Python policy_from_extra()
 *
 *  DM scope falls back to group scope keys ONLY for
 *  user_allowed_commands when the DM scope didn't specify its own.
 * ================================================================ */

slash_policy_t *slash_policy_from_extra(json_node_t *extra, const char *scope) {
    slash_policy_t *policy = calloc(1, sizeof(slash_policy_t));
    if (!policy) return NULL;

    if (!extra) {
        policy->enabled = false;
        return policy;
    }

    const char *admin_key, *cmd_key;
    keys_for_scope(scope, &admin_key, &cmd_key);

    json_node_t *admin_raw = json_object_get(extra, admin_key);
    json_node_t *cmd_raw = json_object_get(extra, cmd_key);

    char *admin_ids = coerce_id_list(admin_raw);
    char *cmds = coerce_command_list(cmd_raw);

    /* DM fallback: if DM didn't specify commands, use group's */
    if (scope && strcmp(scope, "dm") == 0 && (!cmds || !*cmds)) {
        free(cmds);
        json_node_t *group_cmd_raw = json_object_get(extra, "group_user_allowed_commands");
        cmds = coerce_command_list(group_cmd_raw);
    }

    snprintf(policy->admin_user_ids, sizeof(policy->admin_user_ids), "%s",
             admin_ids ? admin_ids : "");
    snprintf(policy->user_allowed_commands, sizeof(policy->user_allowed_commands), "%s",
             cmds ? cmds : "");

    policy->enabled = (admin_ids && *admin_ids);

    free(admin_ids);
    free(cmds);
    return policy;
}

/* ================================================================
 *  Resolve access policy for a session source
 *  Port of Python policy_for_source()
 *
 *  Returns a "disabled" policy (gating off) when:
 *    - gateway_config is NULL
 *    - the platform has no config entry
 *    - the config entry has no admin list for the scope
 *
 *  Caller must free() the returned policy.
 * ================================================================ */

slash_policy_t *slash_policy_for_source(json_node_t *gateway_config,
                                         const gw_session_source_t *source) {
    if (!gateway_config || !source) {
        slash_policy_t *policy = calloc(1, sizeof(slash_policy_t));
        if (policy) policy->enabled = false;
        return policy;
    }

    /* Get platforms dict from gateway_config */
    json_node_t *platforms = json_object_get(gateway_config, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        slash_policy_t *policy = calloc(1, sizeof(slash_policy_t));
        if (policy) policy->enabled = false;
        return policy;
    }

    /* Look up this platform's config */
    json_node_t *platform_config = json_object_get(platforms, source->platform);
    json_node_t *extra = platform_extra(platform_config);

    const char *scope = scope_for_chat_type(source->chat_type);
    slash_policy_t *policy = slash_policy_from_extra(extra, scope);

    json_free(extra);
    return policy;
}
