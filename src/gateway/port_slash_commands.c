/*
 * port_slash_commands.c — Faithful C11 ports of gateway/slash_commands.py
 * (GatewaySlashCommandsMixin) pure-logic / dependency-backed helpers.
 *
 * Reuses existing C subsystems via their opaque public APIs:
 *   - i18n:        i18n_t / i18n_t_fmt          (hermes_i18n.h)
 *   - config I/O:  config_py_atomic_config_write (port_config_py_helpers.h)
 *   - slash gate:  slash_policy_for_source / slash_policy_is_admin
 *                                                (hermes_gateway_slash_access.h)
 *   - config load: yaml_parse_file / yaml_to_json_string (yaml.h)
 */

#include "port_slash_commands.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hermes_i18n.h"
#include "port_config_py_helpers.h"
#include "hermes_gateway_slash_access.h"
#include "yaml.h"

/* hermes_constants.VALID_REASONING_EFFORTS */
static const char *const VALID_REASONING_EFFORTS[] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};

/* lower-case an ASCII string in place. */
static void ascii_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* strip leading/trailing ASCII whitespace; returns a malloc'd copy. */
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ───────────────────── parse_reasoning_effort ─────────────────────
 * PoP: reasoning_parse_effort @ hermes_constants.py:parse_reasoning_effort */
json_t *reasoning_parse_effort(const char *effort, bool effort_is_bool_false) {
    /* effort is False -> {"enabled": False} */
    if (effort_is_bool_false) {
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    /* None / True -> None (True is caller-signalled; here NULL string == None) */
    if (!effort) return NULL;

    char *low = strip_dup(effort);
    if (!low || low[0] == '\0') { free(low); return NULL; }
    ascii_lower(low);

    if (strcmp(low, "none") == 0 || strcmp(low, "false") == 0 ||
        strcmp(low, "disabled") == 0) {
        free(low);
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    for (int i = 0; VALID_REASONING_EFFORTS[i]; i++) {
        if (strcmp(low, VALID_REASONING_EFFORTS[i]) == 0) {
            json_t *o = json_object();
            json_set(o, "enabled", json_bool(true));
            json_set(o, "effort", json_string(low));
            free(low);
            return o;
        }
    }
    free(low);
    return NULL;
}

/* ─────────────────── _reasoning_picker_choices ───────────────────
 * PoP: slash_reasoning_picker_choices @ gateway/slash_commands.py:_reasoning_picker_choices */
static json_t *picker_choice(const char *value, char *label, bool is_current) {
    json_t *o = json_object();
    json_set(o, "value", json_string(value));
    json_set(o, "label", json_string(label ? label : value));
    json_set(o, "is_current", json_bool(is_current));
    return o;
}

json_t *slash_reasoning_picker_choices(const char *current_effort) {
    const char *cur = current_effort ? current_effort : "";
    json_t *arr = json_array();

    /* First entry: "none" */
    char *lbl_none = i18n_t("gateway.reasoning.choice_none");
    json_array_append(arr, picker_choice("none", lbl_none,
                                         strcmp(cur, "none") == 0));
    free(lbl_none);

    /* One entry per valid effort level (label == level). */
    for (int i = 0; VALID_REASONING_EFFORTS[i]; i++) {
        const char *lvl = VALID_REASONING_EFFORTS[i];
        json_array_append(arr, picker_choice(lvl, (char *)lvl,
                                             strcmp(cur, lvl) == 0));
    }

    /* Trailing: reset / show / hide (never current). */
    char *lbl_reset = i18n_t("gateway.reasoning.choice_reset");
    json_array_append(arr, picker_choice("reset", lbl_reset, false));
    free(lbl_reset);
    char *lbl_show = i18n_t("gateway.reasoning.choice_show");
    json_array_append(arr, picker_choice("show", lbl_show, false));
    free(lbl_show);
    char *lbl_hide = i18n_t("gateway.reasoning.choice_hide");
    json_array_append(arr, picker_choice("hide", lbl_hide, false));
    free(lbl_hide);

    return arr;
}

/* ─────────────────── _save_gateway_config_key ───────────────────
 * PoP: slash_save_gateway_config_key @ gateway/slash_commands.py:_save_gateway_config_key */
bool slash_save_gateway_config_key(const char *config_path,
                                   const char *key_path,
                                   json_t *value) {
    if (!config_path || !key_path || key_path[0] == '\0') {
        if (value) json_free(value);
        return false;
    }

    /* Load existing config.yaml -> json object (or empty object). */
    json_t *root = NULL;
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &yerr);
    if (yerr) free(yerr);
    if (doc) {
        char *js = yaml_to_json_string(doc, NULL);
        yaml_free(doc);
        if (js) {
            char *jerr = NULL;
            root = json_parse(js, &jerr);
            free(js);
            if (jerr) free(jerr);
        }
    }
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        root = json_object();   /* user_config = {} */
    }

    /* Walk/create intermediate objects for keys[:-1]. */
    char *keys = strdup(key_path);
    if (!keys) { json_free(root); if (value) json_free(value); return false; }

    /* Split on '.' collecting segments. */
    char *segs[64];
    int nseg = 0;
    for (char *tok = strtok(keys, "."); tok && nseg < 64;
         tok = strtok(NULL, ".")) {
        segs[nseg++] = tok;
    }
    if (nseg == 0) { free(keys); json_free(root); if (value) json_free(value); return false; }

    json_t *cur = root;
    for (int i = 0; i < nseg - 1; i++) {
        json_t *child = json_obj_get(cur, segs[i]);
        if (!child || child->type != JSON_OBJECT) {
            child = json_object();
            json_set(cur, segs[i], child);  /* replaces non-dict/missing */
        }
        cur = child;
    }
    /* current[keys[-1]] = value */
    json_set(cur, segs[nseg - 1], value ? value : json_null());

    free(keys);

    int rc = config_py_atomic_config_write(config_path, root);
    json_free(root);
    return rc == 0;
}

/* ─────────────────── _resume_caller_is_admin ───────────────────
 * PoP: slash_resume_caller_is_admin @ gateway/slash_commands.py:_resume_caller_is_admin */
bool slash_resume_caller_is_admin(json_node_t *gateway_config,
                                  const gw_session_source_t *source) {
    if (!source) return false;
    slash_policy_t *policy = slash_policy_for_source(gateway_config, source);
    if (!policy) return false;
    const char *uid = source->user_id;
    bool result = policy->enabled && uid && uid[0] != '\0' &&
                  slash_policy_is_admin(policy, uid);
    free(policy);
    return result;
}
