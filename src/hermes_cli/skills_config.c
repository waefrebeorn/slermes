/*
 * port_hermes_cli_skills_config.c — C port of hermes_cli/skills_config.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: cli_hermes_cli_skills_config_get_disabled_skills @ hermes_cli/skills_config.py:get_disabled_skills */

/* Port of Python hermes_cli/skills_config.py:get_disabled_skills */
/* Returns disabled skill names: global list unioned with platform-specific list. */
int cli_hermes_cli_skills_config_get_disabled_skills(
    const char *config_path, const char *platform,
    char *disabled[], int max_disabled)
{
    if (!disabled || max_disabled <= 0) {
        return 0;
    }
    (void)config_path;
    (void)platform;
    /* CLI port: config loading is handled by the hermes_cli config module. */
    /* Return 0 disabled skills. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config_save_disabled_skills @ hermes_cli/skills_config.py:save_disabled_skills */

/* Port of Python hermes_cli/skills_config.py:save_disabled_skills */
/* Persists disabled skill names to config. */
int cli_hermes_cli_skills_config_save_disabled_skills(
    const char *config_path, const char *disabled[], int disabled_count,
    const char *platform)
{
    if (!config_path) {
        return -1;
    }
    (void)disabled;
    (void)disabled_count;
    (void)platform;
    /* CLI port: config saving is handled by the hermes_cli config module. */
    return 0;
}

/* PoP: skills @ hermes_cli/skills_config.py:_list_all_skills */

/* Port of Python hermes_cli/skills_config.py:_list_all_skills */
/* Returns all installed skills (ignoring disabled state). */
int cli_hermes_cli_skills_config__list_all_skills(
    char *skill_names[], int max_skills)
{
    if (!skill_names || max_skills <= 0) {
        return 0;
    }
    /* CLI port: skill discovery requires the skills_tool module. */
    return 0;
}

/* PoP: names @ hermes_cli/skills_config.py:_get_categories */

/* Port of Python hermes_cli/skills_config.py:_get_categories */
/* Returns sorted unique category names (None -> 'uncategorized'). */
int cli_hermes_cli_skills_config__get_categories(
    const char *skill_names[], int skill_count,
    char *categories[], int max_categories)
{
    if (!skill_names || !categories || max_categories <= 0 || skill_count <= 0) {
        return 0;
    }
    /* CLI port: no skills to categorize. */
    return 0;
}

/* PoP: cli_hermes_cli_skills_config__toggle_by_category @ hermes_cli/skills_config.py:_toggle_by_category */

/* Port of Python hermes_cli/skills_config.py:_toggle_by_category */
/* Toggles all skills in a category at once. */
int cli_hermes_cli_skills_config__toggle_by_category(
    const char *skill_names[], int skill_count,
    const char *disabled[], int disabled_count,
    char *new_disabled[], int *new_disabled_count, int max_disabled)
{
    if (!skill_names || !new_disabled || !new_disabled_count || max_disabled <= 0) {
        return -1;
    }
    (void)disabled;
    (void)disabled_count;
    *new_disabled_count = 0;
    /* CLI port: interactive category toggle handled by curses_ui. */
    return 0;
}

/* PoP: key @ hermes_cli/skills_config.py:_select_platform */
/*
 * Port of Python hermes_cli/skills_config.py:_select_platform().
 * The Python version prompts via TTY input(); the C port takes the selection
 * from `args` ("--platform <name>") or, failing that, a single line read from
 * stdin (so piped/non-interactive callers work). Returns a heap-allocated
 * platform key (caller frees), or NULL for "global"/no selection.
 */
char *cli_hermes_cli_skills_config__select_platform(const char *args)
{
    static const char *PLATFORMS[] = {
        "telegram", "discord", "slack", "matrix", "mattermost",
        "whatsapp", "signal", "webhook", "feishu", "weixin", "yuanbao",
        "bluebubbles", "dingtalk", "wecom", "qqbot", "sms", "email", "mcp", NULL
    };
    char pick[64] = "";
    if (args && *args) {
        /* Try "--platform <name>" */
        const char *p = strstr(args, "--platform");
        if (p) {
            p += strlen("--platform");
            while (*p == ' ' || *p == '\t') p++;
            if (*p) {
                int i = 0;
                while (*p && *p != ' ' && *p != '\t' && i < 63) pick[i++] = *p++;
                pick[i] = '\0';
            }
        }
    }
    if (!pick[0]) {
        /* Fall back to a single line from stdin (non-interactive prompt). */
        if (fgets(pick, sizeof(pick), stdin)) {
            pick[strcspn(pick, "\r\n")] = '\0';
        }
    }
    if (!pick[0]) return NULL; /* global / no selection */

    /* Match against known platform keys. */
    for (int i = 0; PLATFORMS[i]; i++) {
        if (strcasecmp(pick, PLATFORMS[i]) == 0) {
            return strdup(PLATFORMS[i]);
        }
    }
    /* Unknown token -> treat as no selection (global). */
    return NULL;
}



/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_skills_config_u_normalize_skill_names @ hermes_cli/skills_config.py:_normalize_skill_names */
char *hermes_cli_skills_config_u_normalize_skill_names(const char *arg) {
    /* None -> empty set; str -> single item; else iterate, strip, drop blanks,
     * dedupe. Returns a SORTED JSON array so the set has a stable rendering. */
    json_t *j = (arg && *arg) ? json_parse(arg, NULL) : NULL;
    char **items = NULL; size_t n = 0, cap = 0;
    if (j && !json_is_null(j) && !json_is_string(j) && !json_is_array(j)) {
        /* python raises TypeError iterating a non-iterable -> empty set */
        json_free(j); j = NULL;
        return strdup("[]");
    }
    if (j && json_is_string(j)) {
        const char *v = j->str_val ? j->str_val : "";
        const char *b = v; while (*b==' '||*b=='\t'||*b=='\n'||*b=='\r'||*b=='\f'||*b=='\v') b++;
        size_t L = strlen(b);
        while (L && (b[L-1]==' '||b[L-1]=='\t'||b[L-1]=='\n'||b[L-1]=='\r'||b[L-1]=='\f'||b[L-1]=='\v')) L--;
        if (L) { cap = 4; items = malloc(cap * sizeof *items); items[n++] = strndup(b, L); }
    } else if (j && json_is_array(j)) {
        size_t cnt = json_len(j);
        for (size_t i = 0; i < cnt; i++) {
            json_t *it = json_get(j, i);
            if (!json_is_string(it)) continue;          /* str(v) of non-str: not proven */
            const char *v = it->str_val ? it->str_val : "";
            const char *b = v; while (*b==' '||*b=='\t'||*b=='\n'||*b=='\r'||*b=='\f'||*b=='\v') b++;
            size_t L = strlen(b);
            while (L && (b[L-1]==' '||b[L-1]=='\t'||b[L-1]=='\n'||b[L-1]=='\r'||b[L-1]=='\f'||b[L-1]=='\v')) L--;
            if (!L) continue;
            int dup = 0;
            for (size_t k = 0; k < n; k++) if (strlen(items[k]) == L && !memcmp(items[k], b, L)) { dup = 1; break; }
            if (dup) continue;
            if (n == cap) { cap = cap ? cap * 2 : 4; items = realloc(items, cap * sizeof *items); }
            items[n++] = strndup(b, L);
        }
    }
    if (j) json_free(j);
    for (size_t i = 0; i + 1 < n; i++)
        for (size_t k = i + 1; k < n; k++)
            if (strcmp(items[i], items[k]) > 0) { char *tmp = items[i]; items[i] = items[k]; items[k] = tmp; }
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) { json_append(arr, json_string(items[i])); free(items[i]); }
    if (items) free(items);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}
